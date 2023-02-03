// Pierre - Custom Light Show for Wiss Landing
// Copyright (C) 2021  Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#include "desk.hpp"
#include "async_msg.hpp"
#include "base/input_info.hpp"
#include "base/threads.hpp"
#include "dmx_ctrl.hpp"
#include "dmx_data_msg.hpp"
#include "frame/anchor_last.hpp"
#include "frame/frame.hpp"
#include "frame/racked.hpp"
#include "frame/silent_frame.hpp"
#include "fx/all.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"
#include "lcs/stats.hpp"
#include "mdns/mdns.hpp"

#include <exception>
#include <functional>
#include <future>
#include <latch>
#include <math.h>
#include <optional>
#include <ranges>
#include <tuple>

namespace pierre {

// must be defined in .cpp to hide mdns
Desk::Desk(MasterClock *master_clock) noexcept
    : frame_strand(io_ctx),                                       // serialize spooler frame access
      frame_timer(io_ctx),                                        //
      guard(asio::make_work_guard(io_ctx)),                       //
      master_clock(master_clock),                                 //
      loop_active{false},                                         //
      thread_count(config_val<int>("desk.threads"sv, 2)),         //
      startup_latch(std::make_shared<std::latch>(thread_count)),  //
      shutdown_latch(std::make_shared<std::latch>(thread_count)), //
      started(false)                                              //
{
  INFO(module_id, "init", "sizeof={} lead_time_min={}\n", sizeof(Desk),
       pet::humanize(InputInfo::lead_time_min));

  resume();
}

Desk::~Desk() noexcept {
  static constexpr csv fn_id{"shutdown"};

  INFO(module_id, fn_id, "requested, io_ctx stopped={}\n", io_ctx.stopped());

  guard.reset();
  standby();

  [[maybe_unused]] error_code ec;
  frame_timer.cancel(ec);
  io_ctx.stop();

  for (auto n = 0; (n < 10) && !shutdown_latch->try_wait(); n++) {
    std::this_thread::sleep_for(100ms);
    io_ctx.stop();
  }

  INFO(module_id, fn_id, "complete, io_ctx stopped={}\n", io_ctx.stopped());
}

// general API
void Desk::frame_loop() noexcept {
  static constexpr csv fn_id{"frame_loop"};

  // first things first, ensure we are running in the correct strand
  if (frame_strand.running_in_this_thread() == false) {
    asio::post(frame_strand, std::bind(&Desk::frame_loop, this));

    INFO(module_id, fn_id, "moving self to frame_strand\n");
    return; // do not fall through to the frame procesing loop
  }

  // initialize supporting objects
  if (!racked) racked.emplace(master_clock);
  if (!active_fx) active_fx = std::move(std::make_unique<fx::Standby>(io_ctx));

  if (startup_latch) {
    startup_latch->wait(); // hold for all threads to start before starting loop
    startup_latch.reset(); // reset shared ptr, we're done with it
  }

  loop_active.store(true);
  std::atomic_bool dmx_starting{false};

  // initial sync wait and fx_finished (used by loop below)
  Nanos sync_wait = InputInfo::lead_time_min;
  bool fx_finished{true};

  // frame loop continues until loop_active is false
  // this is typically set once all FX have been executed and the system is
  // officially idle (e.g. no live session)
  while (loop_active.load() && !io_ctx.stopped()) {
    Elapsed next_wait;
    frame_t frame;

    // Racked will always return a frame (from racked or silent)
    try {
      frame = racked->next_frame().get();
    } catch (...) {
      break;
    }

    if (!loop_active) break;

    // we now have a valid frame:
    //  1. rendering = from Racked (peaks or silent)
    //  2. not rendering = generated SilentFrame

    Stats::write(stats::NEXT_FRAME_WAIT, next_wait.freeze());

    frame->state.record_state();

    if (fx_finished) {
      const auto fx_name_now = active_fx ? active_fx->name() : "NONE";
      const auto fx_suggested_next = active_fx ? active_fx->suggested_fx_next() : fx_name::STANDBY;

      active_fx->cancel(); // stop any pending io_ctx work

      if ((fx_suggested_next == fx_name::STANDBY) && frame->silent()) {
        active_fx = std::make_unique<fx::Standby>(io_ctx);

      } else if ((fx_suggested_next == fx_name::MAJOR_PEAK) && !frame->silent()) {
        active_fx = std::make_unique<fx::MajorPeak>(io_ctx);

      } else if (!frame->silent() && (frame->state.ready() || frame->state.future())) {
        active_fx = std::make_unique<fx::MajorPeak>(io_ctx);

      } else if ((fx_suggested_next == fx_name::ALL_STOP) && frame->silent()) {
        active_fx = std::make_unique<fx::AllStop>();

        standby();
        break;

      } else { // default to Standby
        active_fx = std::make_unique<fx::Standby>(io_ctx);
      }

      // note in log switch to new FX, if needed
      if (active_fx->match_name(fx_name_now) == false) {
        INFO(module_id, fn_id, "FX {} -> {}\n", fx_name_now, active_fx->name());
      }
    }

    // render this frame and send to DMX controller
    if (frame->state.ready()) {
      DmxDataMsg msg(frame, InputInfo::lead_time);

      if (fx_finished = active_fx->render(frame, msg); fx_finished == false) {
        if (dmx_ctrl && loop_active) {
          dmx_ctrl->send_data_msg(std::move(msg));

        } else if (loop_active && !dmx_starting) {
          dmx_starting = true;

          asio::post(io_ctx, [this, &dmx_starting]() mutable {
            dmx_ctrl = std::make_shared<DmxCtrl>();
            dmx_starting = false;
          });
        }
      }
    }

    // account for processing time thus far
    sync_wait = frame->sync_wait_recalc();

    // notates rendered or silence in timeseries db
    frame->mark_rendered();

    if (sync_wait >= Nanos::zero() && loop_active) {
      // now we need to wait for the correct time to render the next frame
      frame_timer.expires_after(sync_wait);
      auto timer_fut = frame_timer.async_wait(asio::use_future);

      try {
        timer_fut.get();
      } catch (...) {
        loop_active = false;
      }
    }
  } // while loop

  INFO(module_id, fn_id, "fell through\n");
}

void Desk::resume() noexcept {
  static constexpr csv fn_id{"resume"};

  bool want_started{false};
  auto check_started = std::atomic_compare_exchange_strong(&started, &want_started, true);

  if (check_started == false) return;

  INFO(module_id, fn_id, "initiated, thread_count={}\n", thread_count);

  if (!startup_latch) startup_latch = std::make_shared<std::latch>(thread_count);

  // once io_ctx is running (by starting threads) kick off frame_loop
  asio::post(io_ctx, std::bind(&Desk::frame_loop, this));

  // note: work guard created in constructor
  for (auto n = 0; n < thread_count; n++) {
    std::jthread([this, n = n, startup_latch = startup_latch,
                  shutdown_latch = shutdown_latch]() mutable {
      const auto thread_name = name_thread(TASK_NAME, n);

      startup_latch->count_down();
      startup_latch.reset();

      INFO(module_id, "init", "thread {}\n", thread_name);
      io_ctx.run();

      INFO(module_id, "shutdown", "thread {}\n", thread_name);
      shutdown_latch->count_down();
      shutdown_latch.reset();
    }).detach();
  }

  // all threads / strands are running, fire up subsystems
  INFO(module_id, fn_id, "threads={}\n", thread_count);
}

void Desk::standby() noexcept {
  static constexpr csv fn_id{"standby"};

  bool want_started{true};
  auto check_started = std::atomic_compare_exchange_strong(&started, &want_started, false);

  if (check_started == false) return;

  loop_active.store(false);

  INFO(module_id, fn_id, "requested, io_ctx stopped={}\n", io_ctx.stopped());

  // stop subsystems
  dmx_ctrl.reset();
  active_fx.reset();
  racked.reset();

  INFO(module_id, fn_id, "complete, io_ctx stopped={}\n", io_ctx.stopped());
}
} // namespace pierre
