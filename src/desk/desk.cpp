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

#include "desk/desk.hpp"
#include "base/input_info.hpp"
#include "base/thread_util.hpp"
#include "desk/async_msg.hpp"
#include "dmx_ctrl.hpp"
#include "dmx_data_msg.hpp"
#include "frame/anchor_last.hpp"
#include "frame/frame.hpp"
#include "frame/master_clock.hpp"
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
    : guard(asio::make_work_guard(io_ctx)), //
      frame_timer(io_ctx),                  //
      master_clock(master_clock),           //
      loop_active{false},                   //
      state{Stopped},                       //
      thread_count(config_threads<Desk>(2)) //
{
  INFO_INIT("sizeof={:>5} lead_time_min={}\n", sizeof(Desk),
            pet::humanize(InputInfo::lead_time_min));

  resume();
}

Desk::~Desk() noexcept {
  INFO_SHUTDOWN_REQUESTED();

  standby();

  if (shutdown_latch) shutdown_latch->wait();

  INFO_SHUTDOWN_COMPLETE();
}

// general API
void Desk::frame_loop() noexcept {
  static constexpr csv fn_id{"frame_loop"};

  // NOTE: frame loop contains an actual while() loop to minimize async calls
  //       and only exits when loop_active == false or io_ctx.stopped() == true
  //       this approach also eliminates a strand for syncronizing frame processing
  if (!racked) racked.emplace(master_clock);
  if (!active_fx) active_fx = std::make_unique<fx::Standby>(io_ctx);

  loop_active = true; // loop is going live

  // initial sync wait and fx_finished (used by loop below)
  Nanos sync_wait = InputInfo::lead_time_min;
  bool fx_finished{false};

  // frame loop continues until loop_active is false
  // this is typically set once all FX have been executed and the system is
  // officially idle (e.g. no live session)
  while (loop_active && !io_ctx.stopped()) {
    Elapsed next_wait;
    frame_t frame;

    // Racked will always return a frame (from racked or silent)
    try {
      frame = racked->next_frame().get();
    } catch (...) {
      INFO_AUTO("next_frame exception\n");
      loop_active = false;
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

      // when fx::Standby is finished initiate standby()
      if (fx_suggested_next == fx_name::ALL_STOP) {
        INFO_AUTO("fx::Standby finished, initiating standby\n");
        standby();
        break;

      } else if ((fx_suggested_next == fx_name::STANDBY) && frame->silent()) {
        active_fx = std::make_unique<fx::Standby>(io_ctx);

      } else if ((fx_suggested_next == fx_name::MAJOR_PEAK) && !frame->silent()) {
        active_fx = std::make_unique<fx::MajorPeak>(io_ctx);

      } else if (!frame->silent() && (frame->state.ready() || frame->state.future())) {
        active_fx = std::make_unique<fx::MajorPeak>(io_ctx);

      } else { // default to Standby
        active_fx = std::make_unique<fx::Standby>(io_ctx);
      }

      // note in log switch to new FX, if needed
      if (!active_fx->match_name(fx_name_now)) {
        INFO_AUTO("FX {} -> {}\n", fx_name_now, active_fx->name());
      }
    }

    if (!loop_active) break;

    // render this frame and send to DMX controller
    if (frame->state.ready()) {
      DmxDataMsg msg(frame, InputInfo::lead_time);

      if (fx_finished = active_fx->render(frame, msg); fx_finished == false) {
        if (dmx_ctrl) {
          dmx_ctrl->send_data_msg(std::move(msg));

        } else {
          dmx_ctrl = std::make_unique<DmxCtrl>();

          asio::post(io_ctx, std::bind(&DmxCtrl::run, dmx_ctrl.get()));
        }
      }
    }

    if (!loop_active) break;

    // account for processing time thus far
    sync_wait = frame->sync_wait_recalc();

    // notates rendered or silence in timeseries db
    frame->mark_rendered();

    if (sync_wait >= Nanos::zero() && loop_active) {
      // now we need to wait for the correct time to render the next frame
      frame_timer.expires_after(sync_wait);
      auto timer_fut = frame_timer.async_wait(asio::use_future);

      try {
        while (true) {
          if (!timer_fut.valid() || !loop_active) throw std::runtime_error("invalid future");
          auto fut_status = timer_fut.wait_for(InputInfo::lead_time_min);
          if (fut_status == std::future_status::ready) break;
        }
      } catch (const std::exception &err) {
        INFO_AUTO("timer_fut exception {}\n", err.what());
        loop_active = false;
      }
    }
  } // while loop

  INFO_AUTO("fell through, io_ctx={}\n", io_ctx.stopped());
}

void Desk::resume() noexcept {
  static constexpr csv fn_id{"resume"};

  std::unique_lock lck(run_state_mtx, std::defer_lock);
  lck.lock();

  if (state == Running) return;

  state = Running;

  // the io_ctx may have been stopped since Desk is intended to be allocated
  // once per app run
  if (io_ctx.stopped()) io_ctx.restart();

  INFO_AUTO("requested, thread_count={}\n", thread_count);

  shutdown_latch = std::make_unique<std::latch>(thread_count);

  // submit work to io_ctx
  asio::post(io_ctx, std::bind(&Desk::frame_loop, this));

  // note: work guard created in constructor
  for (auto n = 0; n < thread_count; n++) {
    std::jthread([this, n = n, shut_latch = shutdown_latch.get()]() mutable {
      const auto thread_name = thread_util::set_name(TASK_NAME, n);

      // thread start syncronization not required
      INFO_THREAD_START();
      io_ctx.run();

      shut_latch->count_down();
      INFO_THREAD_STOP();
    }).detach();
  }

  INFO_AUTO("completed, thread_count={}\n", thread_count);
}

void Desk::standby() noexcept {
  static constexpr csv fn_id{"standby"};

  if (state == Stopped) return;

  std::unique_lock lck(run_state_mtx, std::defer_lock);

  // if we can't aquire the lock then simply leave Desk running
  if (lck.try_lock() == false) return;

  // we're committed to stopping now, set the state
  state = Stopped;
  loop_active = false;

  INFO_AUTO("requested, io_ctx={}\n", io_ctx.stopped());

  guard.reset(); // allow io_ctx to run out of work

  // stop frame_timer (very high likely that's where frame_loop is waiting)
  try {
    frame_timer.cancel();
  } catch (...) {
  }

  // shutdown supporting subsystems
  dmx_ctrl.reset();
  active_fx.reset();
  racked.reset();

  if (shutdown_latch) {
    for (auto n = 0; (n < 10) && !shutdown_latch->try_wait(); n++) {
      std::this_thread::sleep_for(50ms);
      io_ctx.stop();
    }
  } else {
    INFO_AUTO("warning: shutdown_latch={}\n", (bool)shutdown_latch);
  }

  INFO_AUTO("completed, io_ctx={}\n", io_ctx.stopped());
}
} // namespace pierre
