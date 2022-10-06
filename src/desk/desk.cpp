/*
    Pierre - Custom Light Show for Wiss Landing
    Copyright (C) 2021  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#include "desk/desk.hpp"
// #include "desk/fx/colorbars.hpp"
// #include "desk/fx/leave.hpp"
#include "ArduinoJson.hpp"
#include "base/anchor_last.hpp"
#include "base/input_info.hpp"
#include "base/uint8v.hpp"
#include "config/config.hpp"
#include "core/host.hpp"
#include "desk/data_msg.hpp"
#include "desk/fx.hpp"
#include "desk/fx/majorpeak.hpp"
#include "desk/fx/silence.hpp"
#include "desk/unit/all.hpp"
#include "desk/unit/opts.hpp"
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
      render_strand(io_ctx),                        // sync frame rendering
      not_rendering_timer(io_ctx),                  //
      active_fx(fx_factory::create<fx::Silence>()), // active FX initializes to Silence
      latency(500us),                               // ruth latency (default)
      guard(io_ctx.get_executor()),                 // prevent io_ctx from ending
      stats(io_ctx, 10s)                            // create the stats object
{}

// general API

void Desk::frame_render(frame_t frame) {

  asio::post(render_strand, [=, this]() {
    desk::DataMsg data_msg(frame, InputInfo::lead_time());

    // frame->mark_rendered(); // frame is consumed, even when no connection

    if ((active_fx->matchName(fx::SILENCE)) && !frame->silent()) {
      active_fx = fx_factory::create<fx::MajorPeak>();
    }

    active_fx->render(frame, data_msg);
    data_msg.finalize();

    if (control) {                       // control exists
      if (control->ready()) [[likely]] { // control is ready

        io::async_write_msg(control->data_socket(), std::move(data_msg),
                            [this](const error_code ec) {
                              stats.frames++;

                              if (ec) {
                                streams_deinit();
                              }
                            });

      } else if (ec_last_ctrl_tx) {
        __LOG0(LCOL01 " shutting down streams, ctrl={}\n", module_id, "ERROR",
               ec_last_ctrl_tx.message());

        streams_deinit();

      } else { // control not ready, increment stats
        stats.no_conn++;
      }
    } else { // control does not exist, initialize it
      streams_init();
    }
  });
}

void Desk::init() { // static instance creation
  if (shared::desk.has_value() == false) {

    mDNS::browse("_ruth._tcp");

    shared::desk.emplace();

    __LOG0(LCOL01 " sizeof={}\n", module_id, "INIT", sizeof(Desk));

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
    __LOG0(LCOL01 " all threads started\n", module_id, "INIT");

    shared::desk->streams_init();
    shared::desk->run();
  }
}

void Desk::run() {

  if (Racked::rendering()) {
    asio::post(frame_strand, [&desk = shared::desk.value()]() mutable {
      Elapsed elapsed;
      auto frame = Racked::next_frame(InputInfo::lead_time()).get();

      if (elapsed() > 10ms) {
        __LOG0(LCOL01 "{} {}\n", module_id, "RUN", Frame::inspect_safe(frame), elapsed.humanize());
      }

      if (frame && frame->state.ready()) desk.frame_render(frame);

      auto sync_wait = (frame) ? frame->sync_wait : InputInfo::lead_time();

      if (sync_wait > Nanos::zero()) {
        desk.frame_timer.expires_after(sync_wait);
        desk.frame_timer.async_wait([&desk = shared::desk.value()](const error_code ec) mutable {
          if (!ec) desk.run();
        });
      } else if (sync_wait == Nanos::zero()) {
        __LOG0(LCOL01 " {}\n", module_id, "SYNC_WAIT_ZERO", Frame::inspect_safe(frame));
        desk.run();
      } else {
        __LOG0(LCOL01 " ABORTING {}\n", module_id, "SYNC_WAIT_MEG", Frame::inspect_safe(frame));
      }
    });

  } else {
    // not rendering

    frame_timer.expires_after(InputInfo::lead_time());
    frame_timer.async_wait([&desk = shared::desk.value()](const error_code ec) mutable {
      if (!ec) desk.run();
    });
  }
}

void Desk::streams_deinit() {
  if (shared::desk->control) {
    asio::post(streams_strand, [this]() { shared::desk->control.reset(); });
  }
}

void Desk::streams_init() {
  if (!shared::desk->control) {
    asio::post(streams_strand, [this]() { // syncronize updates to shared state
      shared::desk->control.emplace(      // create ctrl stream
          shared::desk->io_ctx,           // majority of rx/tx use io_ctx
          streams_strand,                 // shared state/status protected by this strand
          ec_last_ctrl_tx,                // last error_code (updared vis streams_strand)
          InputInfo::lead_time(),         // default lead_time
          stats                           // desk stats
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
    msg = fmt::format("frame timer canceled, rendering={}\n", Racked::rendering());
    break;

  default:
    msg = fmt::format("frame timer error, reason={} rendering={}\n", ec.message(),
                      Racked::rendering());
  }

  if (msg.empty() == false) {
    __LOG0(LCOL01 " {}\n", module_id, fn_id, msg);
  }

  return rc;
}

} // namespace pierre
