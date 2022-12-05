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
#include "base/render.hpp"
#include "config/config.hpp"
#include "ctrl.hpp"
#include "data_msg.hpp"
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
static std::shared_ptr<Desk> self;
static std::shared_ptr<FX> active_fx;
static std::shared_ptr<desk::Ctrl> ctrl;

// must be defined in .cpp to hide mdns
Desk::Desk() noexcept
    : frame_strand(io_ctx),              // serialize spooler frame access
      frame_last{0},                     // last frame processed (steady clock)
      guard(io::make_work_guard(io_ctx)) // prevent io_ctx from ending
{}

// general API
void Desk::frame_loop(const Nanos wait) noexcept {

  // first things first, ensure we are running in the correct strand
  if (frame_strand.running_in_this_thread() == false) {
    asio::post(frame_strand, [=]() { self->frame_loop(wait); });

    return; // do not fall through to the frame procesing loop
  }

  // we running in the correct strand, process frames "forever"
  // via a while loop avoiding unncessary asio::posts
  loop_active = true;
  steady_timer frame_timer(io_ctx);

  while (true) { // breaks out if there is a timer error
    Nanos sync_wait = wait;
    Elapsed render_wait;
    frame_t frame;
    static auto outdated{0};

    // note: reset render_wait to get best possible measurement
    if (render_wait.reset() && Render::enabled()) {
      Stats::write(stats::RENDER_DELAY, render_wait.freeze());

      Elapsed next_wait;

      // Racked will always return either a racked or silent frame
      frame = Racked::next_frame().get();
      Stats::write(stats::NEXT_FRAME_WAIT, next_wait.freeze());
    } else {
      // not rendering, generate a silent frame
      frame = SilentFrame::create();
    }

    frame->state.record_state();

    // we now have a valid frame (shared_ptr):
    //  1. rendering = from Racked (peaks or silent)
    //  2. not rendering = generated SilentFrame

    if (frame->state.ready()) { // render this frame and send to DMX controller

      desk::DataMsg data_msg(frame, InputInfo::lead_time);

      const auto fx_name_now = active_fx->name();
      if (fx_next == fx::LEAVE) {
        fx_next.clear();

        active_fx = std::make_shared<fx::Leave>();

      } else if ((active_fx->match_name({fx::SILENCE, fx::LEAVE})) && !frame->silent()) {

        if (active_fx->match_name(fx::MAJOR_PEAK) == false) {
          active_fx = std::make_shared<fx::MajorPeak>(io_ctx);
        }
      }

      if (active_fx->match_name(fx_name_now) == false) {
        INFO(module_id, "FRAME_RENDER", "FX {} -> {}\n", fx_name_now, active_fx->name());
      }

      auto fx_finished = active_fx->render(frame, data_msg);
      data_msg.finalize();

      ctrl->send_data_msg(std::move(data_msg));

      if (fx_finished) {
        if (fx_name_now == fx::MAJOR_PEAK) {
          fx_next = fx::LEAVE;
        } else if (fx_name_now == fx::SILENCE) {
          fx_next = fx::LEAVE;
        }

        INFO(module_id, "FRAME_LOOP", "FX finished={} active={} next={}\n", fx_finished,
             fx_name_now, fx_next);
      }

      frame_last = pet::now_realtime();
    } else if (frame->state.outdated()) {
      outdated++;

    } else {
      INFO(module_id, "FRAME_LOOP", "NOT RENDERED frame={}\n", frame->inspect());
    }

    // account for processing time thus far
    sync_wait = frame->sync_wait_recalc();

    frame->mark_rendered(); // records rendered or silence in timeseries db

    if (sync_wait >= Nanos::zero()) {
      // Stats::write(stats::SYNC_WAIT, pet::floor(sync_wait), frame->state.tag());
      frame_timer.expires_after(sync_wait);
      error_code ec;
      frame_timer.wait(ec); // loop runs in frame_strand, no async necessary

      if (ec) {
        INFO(module_id, "FRAME_LOOP", "falling through reason={}\n", ec.message());
        break; // timer error, likely due to shutdown, break out
      }
    }
  } // while loop

  loop_active = false;
}

void Desk::init() noexcept { // static instance creation
  if (self.use_count() == 0) {
    self = std::shared_ptr<Desk>(new Desk());
    self->init_self();
  }
}

void Desk::init_self() noexcept {
  // initialize supporting objects
  Stats::init(); // ensure Stats object is initialized
  active_fx = std::make_shared<fx::Silence>();

  const auto num_threads = Config().at("desk.threads"sv).value_or(3);
  std::latch latch(num_threads);

  // note: work guard created by constructor p
  for (auto n = 0; n < num_threads; n++) { // main thread is 0s
    self->threads.emplace_back([=, &s = self, &latch](std::stop_token token) {
      name_thread(TASK_NAME, n);
      s->tokens.add(std::move(token));

      latch.arrive_and_wait();
      s->io_ctx.run();
    });
  }

  // caller thread waits until all tasks are started
  latch.wait();
  log_init(num_threads);

  // all threads / strands are runing, fire up subsystems
  asio::post(io_ctx, [this]() {
    ctrl = desk::Ctrl::create(io_ctx)->init();

    frame_loop(); // start Desk frame processing
  });
}

bool Desk::silence() const noexcept { return active_fx && active_fx->match_name(fx::SILENCE); }

void Desk::shutdown() noexcept {
  // stop all threads
  // release static supporting objects
}

// misc debug and logging API
bool Desk::log_frame_timer(const error_code &ec, csv fn_id) const noexcept {
  auto rc = true;

  if (!ec) {
    string msg;
    auto w = std::back_inserter(msg);

    fmt::format_to(w, "frame timer ");

    switch (ec.value()) {
    case errc::operation_canceled:
      fmt::format_to(w, "CANCELED");
      break;

    default:
      fmt::format_to(w, "ERROR reason={}", ec.message());
    }

    INFO(module_id, fn_id, "{} rendering={}\n", msg, Render::enabled());

    rc = false;
  }

  return rc;
}

void Desk::log_init(int num_threads) const noexcept {
  INFO(module_id, "INIT", "sizeof={} threads={}/{} lead_time_min={}\n", //
       sizeof(Desk),                                                    //
       num_threads,                                                     //
       threads.size(),                                                  //
       pet::humanize(InputInfo::lead_time_min));
}

} // namespace pierre
