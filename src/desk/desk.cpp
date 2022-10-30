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
#include "base/anchor_last.hpp"
#include "base/input_info.hpp"
#include "base/logger.hpp"
#include "base/render.hpp"
#include "config/config.hpp"
#include "desk/data_msg.hpp"
#include "desk/fx.hpp"
#include "desk/fx/majorpeak.hpp"
#include "desk/fx/silence.hpp"
#include "frame/anchor.hpp"
#include "frame/frame.hpp"
#include "frame/master_clock.hpp"
#include "frame/reel.hpp"
#include "io/async_msg.hpp"
#include "mdns/mdns.hpp"

#include <exception>
#include <future>
#include <latch>
#include <math.h>
#include <optional>
#include <ranges>

namespace pierre {
namespace shared {
std::optional<Desk> desk;
}

// must be defined in .cpp to hide mdns
Desk::Desk() noexcept                               //
    : streams_strand(io_ctx),                       // serialize streams init/deinit
      handoff_strand(io_ctx),                       // serial Spooler frame collection
      frame_strand(io_ctx),                         // serialize spooler frame access
      frame_timer(io_ctx),                          // controls when frame is rendered
      frame_late_timer(io_ctx),                     // handle late frames
      frame_last{0},                                // last frame processed (steady clock)
      render_strand(io_ctx),                        // sync frame rendering
      active_fx(fx_factory::create<fx::Silence>()), // active FX initializes to Silence
      latency(500us),                               // ruth latency (default)
      guard(io_ctx.get_executor()),                 // prevent io_ctx from ending
      run_stats(io_ctx)                             // create the stats object
{}

// general API

// primary frame processing entry loop
void Desk::frame_loop(const Nanos wait) {

  if (pet::is_zero(wait)) {
    frame_loop_safe();
  } else if (wait > Nanos::zero()) {
    frame_loop_delay(wait);
  } else if (wait < Nanos::zero()) {
    INFO(module_id, "FRAME_LOOP", "crazy wait {}\n", pet::humanize(wait));
    frame_loop_delay(wait);
  }
}

void Desk::frame_loop_delay(const Nanos wait) {
  frame_timer.expires_after(wait);
  frame_timer.async_wait([this](const error_code ec) {
    if (log_frame_timer_error(ec, "FRAME_LOOP")) {
      frame_loop_safe();
    }
  });
}

// if code is running in the same thread as frame_strand, avoid async::post
void Desk::frame_loop_safe() {
  if (frame_strand.running_in_this_thread()) {
    frame_loop_unsafe();
  } else {
    asio::post(frame_strand, [this]() { frame_loop_unsafe(); });
  }
}

// must ensure this function only runs on the same thread as frame_strand
void Desk::frame_loop_unsafe() {
  Elapsed delay;
  Render::enabled(true /* wait */);
  run_stats(desk::RENDER_DELAY, delay);

  Elapsed elapsed;
  auto frame = Racked::next_frame().get();
  run_stats(desk::NEXT_FRAME, elapsed);

  // INFO(module_id, "FRAME_LOOP2", "frame={}\n", Frame::inspect_safe(frame));

  if (frame && frame->state.ready()) {
    frame_render(frame);
  } else {
    INFO(module_id, "FRAME_LOOP", "DROP frame={}\n", Frame::inspect_safe(frame));
  }

  auto wait = (frame && frame->sync_wait_ok()) ? frame->sync_wait() : InputInfo::lead_time_min;

  run_stats(desk::SYNC_WAIT, wait);

  frame_loop(wait);
}

void Desk::frame_render(frame_t frame) {

  desk::DataMsg data_msg(frame, InputInfo::lead_time);

  frame->mark_rendered(); // frame is consumed, even when no connection

  if ((active_fx->matchName(fx::SILENCE)) && !frame->silent()) {
    active_fx = fx_factory::create<fx::MajorPeak>(run_stats);
  }

  active_fx->render(frame, data_msg);
  data_msg.finalize();

  if (control) {                       // control exists
    if (control->ready()) [[likely]] { // control is ready

      io::async_write_msg(control->data_socket(), std::move(data_msg), [this](const error_code ec) {
        // run_stats(desk::FRAMES);

        if (ec) {
          streams_deinit();
        }
      });

    } else if (ec_last_ctrl_tx) {
      INFO(module_id, "ERROR", "shutting down streams, ctrl={}\n", ec_last_ctrl_tx.message());

      streams_deinit();

    } else { // control not ready, increment stats
      run_stats(desk::NO_CONN);
    }
  } else { // control does not exist, initialize it
    streams_init();
  }
  // });
}

void Desk::init() { // static instance creation
  if (shared::desk.has_value() == false) {
    shared::desk.emplace().init_self();
  }
}

void Desk::init_self() {
  std::latch latch(DESK_THREADS);

  // note: work guard created by constructor p
  for (auto n = 0; n < DESK_THREADS; n++) { // main thread is 0s
    shared::desk->threads.emplace_back([=, &latch]() mutable {
      name_thread(TASK_NAME, n);
      latch.arrive_and_wait();
      shared::desk->io_ctx.run();
    });
  }

  // caller thread waits until all tasks are started
  latch.wait();
  INFO(module_id, "INIT", "threads started={} sizeof={}\n", //
       shared::desk->threads.size(), sizeof(Desk));

  // start frame loop, start on any thread, waits for rendering
  asio::post(io_ctx, [this]() {
    streams_init();
    frame_loop();
  });
}

void Desk::streams_deinit() {
  if (shared::desk->control) {
    asio::post(streams_strand, [this]() {
      run_stats(desk::STREAMS_DEINIT);

      shared::desk->control.reset();
    });
  }
}

void Desk::streams_init() {
  if (!shared::desk->control) {
    asio::post(streams_strand, [this]() {
      run_stats(desk::STREAMS_INIT);

      shared::desk->control.emplace( // create ctrl stream
          shared::desk->io_ctx,      // majority of rx/tx use io_ctx
          streams_strand,            // shared state/status protected by this strand
          ec_last_ctrl_tx,           // last error_code (updared vis streams_strand)
          InputInfo::lead_time,      // default lead_time
          run_stats                  // desk stats
      );
    });
  }
}

// misc debug and logging API
bool Desk::log_frame_timer_error(const error_code &ec, csv fn_id) const {
  auto rc = false;
  string msg;

  switch (ec.value()) {
  case errc::success:
    rc = true;
    break;

  case errc::operation_canceled:
    msg = fmt::format("frame timer canceled, rendering={}\n", Render::enabled());
    break;

  default:
    msg = fmt::format("frame timer error, reason={} rendering={}\n", //
                      ec.message(), Render::enabled());
  }

  if (msg.empty() == false) INFO(module_id, fn_id, "{}\n", msg);

  return rc;
}

} // namespace pierre
