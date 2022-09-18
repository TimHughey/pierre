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
#include "frame/master_clock.hpp"
#include "io/async_msg.hpp"
#include "mdns/mdns.hpp"
#include "frame/spooler.hpp"

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

// static free functions to limit visibility of Anchor
// static Nanos frame_calc_state(shFrame frame, AnchorLast &anchor_last) {}

// must be defined in .cpp to hide mdns
Desk::Desk(const Nanos &lead_time)
    : lead_time(lead_time),                         // despool frame lead time
      lead_time_50(pet::percent(lead_time, 50)),    // useful constant
      frame_timer(io_ctx),                          // controls when frame is despooled
      streams_strand(io_ctx),                       // serialize serialize streams init/deinit
      release_timer(io_ctx),                        // controls when frame is sent
      active_fx(fx_factory::create<fx::Silence>()), // active FX initializes to Silence
      latency(500us),                               // ruth latency (default)
      guard(io_ctx.get_executor()),                 // prevent io_ctx from ending
      render_mode(NOT_RENDERING),                   // render or don't render
      stats(io_ctx, 10s)                            // create the stats object
{}

// general API
void Desk::adjust_mode(csv next_mode) {
  render_mode = next_mode;

  if (render_mode == RENDERING) {
    if (anchor_available == false) {
      shared::master_clock->async_wait_ready(
          []([[maybe_unused]] const ClockInfo &clock, bool ready) -> void {
            asio::post(shared::desk->io_ctx, [=] {
              if (ready && clock.is_minimum_age()) {
                shared::desk->anchor_available = ready;
                shared::desk->frame_first();
              }
            });
          });
    } else {
      frame_first();
    }

    stats.async_report(5ms);
  } else {
    frame_timer.cancel();
    stats.cancel();
  }
}

void Desk::frame_first() {
  static constexpr csv fn_id{"FRAME_FIRST"};

  auto anchor = shared::anchor->get_data();

  if (anchor.viable()) {
    if (rendering()) {
      shared::spooler->async_head_frame( //
          io_ctx, anchor, lead_time, [lag = Elapsed(), this](shFrame frame) mutable {
            auto sync_wait = frame->sync_wait;

            __LOG0(LCOL01 " first frame sync_wait={}, lag={}\n", module_id, fn_id,
                   pet::humanize(sync_wait), pet::humanize(lag()));

            frame_timer.expires_after(sync_wait);
            frame_timer.async_wait([this](const error_code ec) {
              if (log_frame_timer_error(ec, fn_id)) {
                frame_next();
              } else {
                adjust_mode(NOT_RENDERING);
              }
            });
          });
    } else {
      __LOG0(LCOL01 " rendering={}, next_frame() will not be invoked\n", //
             module_id, fn_id, rendering());
    }
  } else {
    __LOG0(LCOL01 " anchor not available\n", module_id, fn_id);
  }
}

void Desk::frame_next() {
  static csv fn_id = "FRAME_NEXT";

  if (rendering()) {

    auto anchor = shared::anchor->get_data();
    shared::spooler->async_next_frame(
        io_ctx, lead_time, anchor, [anchor, lag = Elapsed(), this](shFrame frame) mutable {
          if (log_despooled(frame, lag())) {

            auto [valid, sync_wait, renderable] = frame->state_now(anchor, lead_time);

            if (valid && renderable) {
              frame_timer.expires_after(sync_wait);
              frame_timer.async_wait([this, frame](const error_code ec) {
                if (log_frame_timer_error(ec, fn_id)) {
                  // no error, render the frame
                  frame_render(frame); // calls frame_next() to frame processing
                  frame_next();
                }
              });
            }

          } else {
            // frame was null, stop rendering
            adjust_mode(NOT_RENDERING);

            __LOG0(LCOL01 " no frame, falling through, rendering={}\n", //
                   module_id, fn_id, rendering());
          }
        });
  }
}

void Desk::frame_render(shFrame frame) {
  desk::DataMsg data_msg(frame, lead_time);

  frame->mark_rendered(); // frame is consumed, even when no connection

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
      __LOG0(LCOL01 " shutting down streams, ctrl={}\n", moduleID(), "ERROR",
             ec_last_ctrl_tx.message());

      streams_deinit();

    } else { // control not ready, increment stats
      stats.no_conn++;
    }
  } else { // control does not exist, initialize it
    streams_init();
  }
}

/*

void Desk::frame_next(Nanos sync_wait) {
  static csv fn_id = "FRAME_NEXT";

  if (rendering()) {

    frame_timer.expires_after(sync_wait);
    frame_timer.async_wait([this](const error_code &ec) {
      if (!ec) {
        shared::spooler->async_head_frame( //
          io_ctx, [lag = Elapsed(), this](shFrame frame) mutable {
              log_despooled(frame, lag());

              Nanos sync_wait = pet::percent(lead_time, 33);

              if (frame) {
                if (frame->sync_wait_valid()) {
                  sync_wait = frame->sync_wait;
                }

                if (frame->renderable()) { // renderable frame
                  frame_render(frame);
                } else if (frame->future()) {
                  ++stats.futures;
                } else {
                  ++stats.none;
                }
              } else { // no frame available
                ++stats.none;
              }

              frame_next(sync_wait);
            });
      } else {
        __LOG0(LCOL01 " rendering={} reason={}\n", moduleID(), fn_id, rendering(), ec.message());
      }
    });
  }
}

*/

void Desk::init(const Nanos &lead_time) { // static instance creation
  if (shared::desk.has_value() == false) {

    mDNS::browse("_ruth._tcp");

    shared::desk.emplace(lead_time);

    __LOG0(LCOL01 " sizeof={}\n", moduleID(), "INIT", sizeof(Desk));

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
  }

  // init the master clock on Airplay io_ctx
  MasterClock::init(shared::desk->io_ctx);

  Anchor::init();

  shared::desk->streams_init();
  // ensure spooler is started
  Spooler::init();
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
          lead_time,                      // default lead_time
          stats                           // desk stats
      );
    });
  }
}

// misc debug and logging API
bool Desk::log_despooled(shFrame frame, const Nanos elapsed) {
  static uint64_t no_playable = 0;
  static uint64_t playable = 0;

  string msg;
  auto w = std::back_inserter(msg);

  if (frame) {
    no_playable = 0;
    ++playable;
    //  if ((frame->sync_wait < 10ms) && ((playable % 1000) == 0)) {
    fmt::format_to(w, " seq_num={} sync_wait={} state={}", frame->seq_num,
                   pet::humanize(frame->sync_wait), Frame::stateVal(frame));
    // }
  } else {
    playable = 0;
    ++no_playable;
    if ((no_playable == 1) || ((no_playable % 100) == 0)) {
      fmt::format_to(w, " no playable frames={:<6}", no_playable);
    }
  }

  if (elapsed >= pet::percent(lead_time, 10)) {
    fmt::format_to(w, " elapsed={:0.2}", pet::as_millis_fp(elapsed));
  }

  if (!msg.empty()) {
    __LOG0(LCOL01 "{}\n", moduleID(), "DESPOOLED", msg);
  }

  return frame.use_count() > 0;
}

bool Desk::log_frame_timer_error(const error_code &ec, csv fn_id) const {
  auto rc = false;

  switch (ec.value()) {
  case errc::success:
    rc = true;
    break;

  case errc::operation_canceled:
    __LOG0(LCOL01 " frame timer canceled, rendering={}\n", module_id, fn_id, rendering());
    break;

  default:
    __LOG0(LCOL01 " frame timer error, reason={} rendering={} \n", module_id, fn_id, ec.message(),
           rendering());
  }

  return rc;
}

} // namespace pierre
