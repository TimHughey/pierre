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

// allow shorthand access to shared objects
std::shared_ptr<Desk> Desk::self;

// must be defined in .cpp to hide mdns
Desk::Desk() noexcept
    : frame_strand(io_ctx),                 // serialize spooler frame access
      guard(asio::make_work_guard(io_ctx)), // prevent io_ctx from ending
      loop_active{true}                     // frame loop defaults to active at creation
{}

// general API
void Desk::frame_loop(const Nanos wait) noexcept {
  static constexpr csv fn_id{"frame_loop"};

  // first things first, ensure we are running in the correct strand
  if (frame_strand.running_in_this_thread() == false) {
    asio::post(frame_strand, [=, s = self->shared_from_this()] { s->frame_loop(wait); });

    return; // do not fall through to the frame procesing loop
  }

  // we running in the correct strand, process frames "forever"
  // via a while loop avoiding unncessary asio::posts
  steady_timer frame_timer(io_ctx);

  // frame loop continues until loop_active is false
  // this is typically set once all FX have been executed and the system is
  // officially idle (no RTSP connection)
  while (loop_active.load() && (io_ctx.stopped() == false)) {
    Elapsed next_wait;
    frame_t frame;
    Nanos sync_wait = wait;

    // Racked will always return either a racked or silent frame
    frame = Racked::next_frame().get();

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
        active_fx = std::make_shared<fx::Standby>(io_ctx);

      } else if ((fx_suggested_next == fx_name::MAJOR_PEAK) && !frame->silent()) {
        active_fx = std::make_shared<fx::MajorPeak>(io_ctx);

      } else if (!frame->silent() && (frame->state.ready() || frame->state.future())) {
        active_fx = std::make_shared<fx::MajorPeak>(io_ctx);

      } else if ((fx_suggested_next == fx_name::ALL_STOP) && frame->silent()) {
        active_fx = std::make_shared<fx::AllStop>();

        loop_active.store(false); // signal to end frame loop

      } else { // default to Standby
        active_fx = std::make_shared<fx::Standby>(io_ctx);
      }

      // note in log switch to new FX, if needed
      if (active_fx->match_name(fx_name_now) == false) {
        INFO(module_id, fn_id, "FX {} -> {}\n", fx_name_now, active_fx->name());
      }
    }

    if (loop_active.load() == false) {
      shutdown();
      break;
    }

    // render this frame and send to DMX controller
    if (frame->state.ready()) {
      DmxDataMsg msg(frame, InputInfo::lead_time);

      if (fx_finished = active_fx->render(frame, msg); fx_finished == false) {
        if (dmx_ctrl.use_count() > 0) {

          dmx_ctrl->send_data_msg(std::move(msg));

        } else {
          asio::post(io_ctx,
                     [s = shared_from_this()]() { //
                       s->dmx_ctrl = DmxCtrl::create(s->io_ctx);
                     });
        }
      }
    }

    // account for processing time thus far
    sync_wait = frame->sync_wait_recalc();

    // notates rendered or silence in timeseries db
    frame->mark_rendered();

    if (sync_wait >= Nanos::zero() && loop_active.load()) {
      // now we need to wait for the correct time to render the next frame
      frame_timer.expires_after(sync_wait);
      error_code ec;
      frame_timer.wait(ec); // loop runs in frame_strand, no async necessary

      if (ec) {
        INFO(module_id, fn_id, "falling through reason={}\n", ec.message());
        loop_active.store(false);
      }
    }
  } // while loop
}

void Desk::init() noexcept {
  if (self.use_count() < 1) {
    self = std::shared_ptr<Desk>(new Desk());

    // grab reference to minimize shared_ptr dereferences
    auto &io_ctx = self->io_ctx;

    // initialize supporting objects
    self->active_fx = std::make_shared<fx::Standby>(io_ctx);
    Racked::init();

    const auto thread_count = config_val<int>("desk.threads"sv, 3);
    auto latch = std::make_shared<std::latch>(thread_count);

    // note: work guard created by constructor
    for (auto n = 0; n < thread_count; n++) {
      self->threads.emplace_back([latch, s = self->ptr(), n = n]() mutable {
        name_thread(TASK_NAME, n);

        latch->arrive_and_wait();
        latch.reset();

        s->io_ctx.run();
      });
    }

    // caller thread waits until all tasks are started
    latch->wait();

    INFO(module_id, "init", "sizeof={} threads={}/{} lead_time_min={}\n", //
         sizeof(Desk),                                                    //
         thread_count,                                                    //
         self->threads.size(),                                            //
         pet::humanize(InputInfo::lead_time_min));

    // all threads / strands are running, fire up subsystems
    asio::post(io_ctx, [s = self->ptr()]() {
      s->dmx_ctrl = DmxCtrl::create(s->io_ctx)->init();

      s->frame_loop(); // start Desk frame processing
    });
  }
}

void Desk::shutdown(bool wait_for_shutdown) noexcept {
  static constexpr csv fn_id{"shutdown"};

  if (self.use_count() > 0) {

    auto s = ptr();
    self.reset();

    INFO(module_id, fn_id, "requested, self.use_count({})\n", s.use_count());

    // must spawn a new thread since Desk could be shutting itself down
    auto latch = std::make_shared<std::latch>(1);
    auto thread = std::jthread([s = std::move(s), latch = latch]() mutable {
      INFO(module_id, fn_id, "initiated, threads={} use_count={}\n", //
           std::ssize(s->threads), s.use_count());

      s->loop_active.store(false);
      s->io_ctx.stop();

      std::erase_if(s->threads, [s = s](auto &t) mutable {
        INFO(module_id, fn_id, "joining thread={} use_count={}\n", //
             t.get_id(), s.use_count());

        t.join();

        return true;
      });

      s.reset();

      INFO(module_id, fn_id, "complete,  use_count={}\n", s.use_count());

      Racked::shutdown();

      latch->count_down();
    });

    if (wait_for_shutdown) {
      latch->wait();
    } else {
      thread.detach();
    }
  }
}

} // namespace pierre
