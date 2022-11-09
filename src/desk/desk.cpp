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
      guard(io::make_work_guard(io_ctx)),           // prevent io_ctx from ending
      run_stats(io_ctx)                             // create the stats object
{}

// general API
void Desk::frame_loop(const Nanos wait) noexcept {
  Nanos sync_wait = wait;
  error_code ec;

  // ensure we are running in the correct strand
  if (frame_strand.running_in_this_thread()) {
    Elapsed render_wait;

    while (!ec) { // loop until error returned by frame_timer

      // note: reset render_wait to get best possible measurement
      if (render_wait.reset() && Render::enabled()) {
        run_stats(desk::RENDER_DELAY, render_wait.freeze());

        Elapsed elapsed_next;
        auto frame = Racked::next_frame().get();
        run_stats(desk::NEXT_FRAME, elapsed_next.freeze());

        if (frame) { // we have an actual frame
          if (frame->state.ready()) {
            frame_render(frame);
            frame_last = pet::now_monotonic();
          } else {
            INFO(module_id, "FRAME_LOOP", "DROP frame={}\n", frame->inspect());
          }

          // we have an actual frame, use it's sync wait
          sync_wait = frame->sync_wait();

        } else { // not rendering
          sync_wait = InputInfo::lead_time_min;
        }

        run_stats(desk::SYNC_WAIT, pet::floor(sync_wait)); // log sync_wait
      }

      ec = error_code(); // reset ec (we may not need to wait)
      if (sync_wait > Nanos::zero()) {
        frame_timer.expires_after(sync_wait);
        frame_timer.wait(ec); // loop runs in frame_strand, no async necessary
      }
    } // while loop

  } else if (!frame_strand.running_in_this_thread()) { // get us to the frame strand
    asio::post(frame_strand, [=, &desk = shared::desk]() { desk->frame_loop(wait); });
  } else {
    INFO(module_id, "FRAME_LOOP", "falling through reason={}\n", ec.message());
  }
}

void Desk::frame_render(frame_t frame) {
  desk::DataMsg data_msg(frame, InputInfo::lead_time);
  frame->mark_rendered(); // frame is consumed, even when no connection

  if ((active_fx->matchName(fx::SILENCE)) && !frame->silent()) {
    active_fx = fx_factory::create<fx::MajorPeak>(run_stats);
  } else if (frame->silent()) {
    run_stats(desk::FRAMES_SILENT);
  } else {
    run_stats(desk::FRAMES_RENDERED);
  }

  active_fx->render(frame, data_msg);
  data_msg.finalize();

  if (control) {                       // control exists
    if (control->ready()) [[likely]] { // control is ready

      io::async_write_msg(        //
          control->data_socket(), //
          std::move(data_msg),    //
          [&desk = shared::desk, elapsed = Elapsed()](const error_code ec) {
            desk->run_stats(desk::REMOTE_ELAPSED, elapsed());
            desk->run_stats(desk::FRAMES);

            if (ec) desk->streams_deinit();
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
}

void Desk::init() { // static instance creation
  if (shared::desk.has_value() == false) {
    shared::desk.emplace().init_self();
  }
}

void Desk::init_self() {
  const auto num_threads = Config().at("desk.threads"sv).value_or(3);
  std::latch latch(num_threads);

  // note: work guard created by constructor p
  for (auto n = 0; n < num_threads; n++) { // main thread is 0s
    shared::desk->threads.emplace_back(
        [&n = n, &desk = shared::desk, &latch](std::stop_token token) {
          name_thread(TASK_NAME, n);
          desk->tokens.add(std::move(token));

          latch.count_down();
          desk->io_ctx.run();
        });
  }

  // caller thread waits until all tasks are started
  latch.wait();
  log_init(num_threads);

  asio::post(io_ctx, [this]() { streams_init(); });     // stream init on any thread
  asio::post(frame_strand, [this]() { frame_loop(); }); // frame loop on frame thread
}

void Desk::streams_deinit() {
  if (control) {
    asio::post(streams_strand, [&desk = shared::desk]() {
      desk->run_stats(desk::STREAMS_DEINIT);

      desk->control.reset();
    });
  }
}

void Desk::streams_init() {
  if (!control) {
    asio::post(streams_strand, [&desk = shared::desk]() {
      desk->run_stats(desk::STREAMS_INIT);

      desk->control.emplace(     // create ctrl stream
          desk->io_ctx,          // majority of rx/tx use io_ctx
          desk->streams_strand,  // shared state/status protected by this strand
          desk->ec_last_ctrl_tx, // last error_code (updated via streams_strand)
          InputInfo::lead_time,  // default lead_time
          desk->run_stats        // desk stats
      );
    });
  }
}

// misc debug and logging API
bool Desk::log_frame_timer(const error_code &ec, csv fn_id) const {
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
