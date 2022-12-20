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
#include "base/input_info.hpp"
#include "base/logger.hpp"
#include "config/config.hpp"
#include "dmx_ctrl.hpp"
#include "dmx_data_msg.hpp"
#include "frame/anchor_last.hpp"
#include "frame/frame.hpp"
#include "frame/racked.hpp"
#include "frame/silent_frame.hpp"
#include "fx/all.hpp"
#include "io/async_msg.hpp"
#include "mdns/mdns.hpp"
#include "stats/stats.hpp"

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
    : frame_strand(io_ctx),               // serialize spooler frame access
      guard(io::make_work_guard(io_ctx)), // prevent io_ctx from ending
      loop_active{true}                   // frame loop defaults to active at creation
{}

// general API
void Desk::frame_loop(const Nanos wait) noexcept {

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
  while (loop_active.load()) {
    Elapsed next_wait;
    frame_t frame;
    Nanos sync_wait = wait;

    // Racked will always return either a racked or silent frame
    frame = Racked::next_frame().get();
    Stats::write(stats::NEXT_FRAME_WAIT, next_wait.freeze());

    frame->state.record_state();

    // we now have a valid frame:
    //  1. rendering = from Racked (peaks or silent)
    //  2. not rendering = generated SilentFrame

    if (fx_finished) {
      const auto fx_name_now = active_fx ? active_fx->name() : "NONE";
      const auto fx_suggested_next = active_fx ? active_fx->suggested_fx_next() : fx_name::STANDBY;

      if ((fx_suggested_next == fx_name::STANDBY) && frame->silent()) {
        active_fx = std::make_shared<fx::Standby>(io_ctx);

      } else if ((fx_suggested_next == fx_name::MAJOR_PEAK) && !frame->silent()) {
        active_fx = std::make_shared<fx::MajorPeak>(io_ctx);

      } else if (!frame->silent() && (frame->state.ready() || frame->state.future())) {
        active_fx = std::make_shared<fx::MajorPeak>(io_ctx);

      } else if ((fx_suggested_next == fx_name::ALL_STOP) && frame->silent()) {
        active_fx = std::make_shared<fx::AllStop>();
        DmxCtrl::teardown(dmx_ctrl); // special case, stop Ctrl

      } else { // default to Standby
        active_fx = std::make_shared<fx::Standby>(io_ctx);
      }

      if (active_fx->match_name(fx_name_now) == false) {
        INFO(module_id, "FRAME_LOOP", "FX {} -> {}\n", fx_name_now, active_fx->name());
      }
    }

    if (frame->state.ready()) { // render this frame and send to DMX controller
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

    if (sync_wait >= Nanos::zero()) {
      // now we need to wait for the correct time to render the next frame
      frame_timer.expires_after(sync_wait);
      error_code ec;
      frame_timer.wait(ec); // loop runs in frame_strand, no async necessary

      if (ec) {
        INFO(module_id, "FRAME_LOOP", "falling through reason={}\n", ec.message());
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

    const auto num_threads = Config().at("desk.threads"sv).value_or(3);
    std::latch latch(num_threads);

    // note: work guard created by constructor
    for (auto n = 0; n < num_threads; n++) {
      self->threads.emplace_back([s = self->ptr(), n = n, &latch]() {
        name_thread(TASK_NAME, n);

        latch.arrive_and_wait();
        s->io_ctx.run();
      });
    }

    // caller thread waits until all tasks are started
    latch.wait();
    self->log_init(num_threads);

    // all threads / strands are runing, fire up subsystems
    asio::post(io_ctx, [s = self->ptr()]() {
      s->dmx_ctrl = DmxCtrl::create(s->io_ctx)->init();

      s->frame_loop(); // start Desk frame processing
    });
  }
}

void Desk::shutdown() noexcept {
  // stop all threads
  // release static supporting objects
}

// misc debug and logging API
void Desk::log_init(int num_threads) const noexcept {
  INFO(module_id, "INIT", "sizeof={} threads={}/{} lead_time_min={}\n", //
       sizeof(Desk),                                                    //
       num_threads,                                                     //
       threads.size(),                                                  //
       pet::humanize(InputInfo::lead_time_min));
}

} // namespace pierre
