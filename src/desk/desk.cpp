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
#include "base/uint8v.hpp"
#include "config/config.hpp"
#include "desk/data_msg.hpp"
#include "desk/fx.hpp"
#include "desk/fx/majorpeak.hpp"
#include "desk/fx/silence.hpp"
#include "desk/unit/all.hpp"
#include "desk/unit/opts.hpp"
#include "io/async_msg.hpp"
#include "mdns/mdns.hpp"
#include "rtp_time/anchor.hpp"
#include "spooler/spooler.hpp"

#include <future>
#include <latch>
#include <math.h>
#include <ranges>

namespace pierre {
namespace desk { // instance
std::unique_ptr<Desk> i;
} // namespace desk

Desk *idesk() { return desk::i.get(); }

// must be defined in .cpp to hide mdns
Desk::Desk(const Nanos &lead_time)
    : lead_time(lead_time),                         // despool frame lead time
      frame_strand(io_ctx),                         // local strand to serialize work
      frame_timer(io_ctx),                          // controls when frame is despooled
      streams_strand(io_ctx),                       // serialize serialize streams init/deinit
      release_timer(io_ctx),                        // controls when frame is sent
      active_fx(fx_factory::create<fx::Silence>()), // active FX initializes to Silence
      latency(5ms),                                 // ruth latency (default)
      work_buff(1024),                              // pre-allocate work buff
      guard(io_ctx.get_executor()),                 // prevent io_ctx from ending
      play_mode(NOT_PLAYING),                       // render or don't render
      stats(io_ctx, 10s) {}

// general API
void Desk::adjust_play_mode(csv next_mode) {
  play_mode = next_mode;

  if (play_mode == PLAYING) {
    frame_next(); // starts despooling frames
    stats.async_report(5ms);
  } else {
    frame_timer.cancel();
    stats.cancel();
  }
}

void Desk::frame_render(shFrame frame) {
  auto data_msg = std::make_unique<desk::DataMsg>(frame);

  if ((active_fx->matchName(fx::SILENCE)) && !frame->silence()) {
    active_fx = fx_factory::create<fx::MajorPeak>();
  }

  active_fx->render(frame, data_msg.get());

  if (control) {                       // control exists
    if (control->ready()) [[likely]] { // control is ready

      auto buff_seq = data_msg->buff_seq();

      control->data_socket().async_write_some(
          buff_seq, [dm = std::move(data_msg)](const error_code ec, size_t bytes) mutable {
            dm->log_tx(ec, bytes);
            dm.reset();
          });

    } else if (!ec_last_ctrl_tx) {
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

void Desk::frame_next(Nanos sync_wait, Nanos lag) {
  static csv fn_id = "FRAME_NEXT";

  if (playing()) {
    frame_timer.expires_after(sync_wait);
    frame_timer.async_wait( // wait the required sync duration before next frame
        asio::bind_executor(frame_strand, [=, this](const error_code &ec) mutable {
          if (!ec) {
            Elapsed elapsed;

            auto future = ispooler()->next_frame(lead_time, lag);
            auto status = future.wait_for(sync_wait - lag - latency); // waits for future
            auto frame = status == std::future_status::ready ? future.get() : shFrame();

            if (status == std::future_status::ready) {
              if (frame && frame->playable()) {
                sync_wait = frame->calc_sync_wait(lag); // recalc to include spooler lag

                // mark played and immediately send the frame to Desk
                frame_release(frame);
                stats.frames++;
              } else {
                sync_wait = Nanos(lead_time / 2); // prevent frame skips, wait 50% of lead time
                stats.none++;
              }
            } else if (status == std::future_status::timeout) { // wait 25% of lead time
              sync_wait = Nanos(lead_time / 4);
              stats.timeouts++;
            }

            log_despooled(frame, elapsed);

            // use sync_wait calculated above to wait for the next frame and the
            // elapsed time as the lag
            frame_next(sync_wait, elapsed());
          } else {
            __LOG0(LCOL01 " playing={} reason={}\n", moduleID(), fn_id, playing(), ec.message());
          }
        }));
  }
}

void Desk::frame_release(shFrame frame) {
  frame->mark_played();

  // recalc the sync wait to account for lag
  auto sync_wait = frame->calc_sync_wait(latency);

  sync_wait = (sync_wait < Nanos::zero()) ? Nanos::zero() : std::move(sync_wait);

  // schedule the release
  release_timer.expires_after(sync_wait);
  release_timer.async_wait([this, frame = frame](const error_code &ec) {
    if (!ec) {
      frame_render(frame);
    }
  });
}

void Desk::init(const Nanos &lead_time) { // static instance creation
  if (auto self = desk::i.get(); !self) {

    mDNS::browse("_ruth._tcp");

    self = new Desk(lead_time);
    desk::i = std::move(std::unique_ptr<Desk>(self));

    __LOG0(LCOL01 " ptr={} sizeof={}\n", moduleID(), "INIT", fmt::ptr(self), sizeof(Desk));

    std::latch latch(DESK_THREADS);

    // note: work guard created by constructor
    for (auto n = 0; n < DESK_THREADS; n++) { // main thread is 0s
      self->threads.emplace_back([=, &latch]() mutable {
        name_thread(TASK_NAME, n);
        latch.arrive_and_wait();
        self->io_ctx.run();
      });
    }

    // caller thread waits until all tasks are started
    latch.wait();
  }

  desk::i->streams_init();

  // ensure spooler is started
  Spooler::init();
}

void Desk::streams_deinit() {
  if (desk::i->control) {
    asio::post(streams_strand, [this]() { desk::i->control.reset(); });
  }
}

void Desk::streams_init() {
  if (!desk::i->control) {
    asio::post(streams_strand, [this]() { // syncronize updates to shared state
      desk::i->control.emplace(           // create ctrl stream
          desk::i->io_ctx,                // majority of rx/tx use io_ctx
          streams_strand,                 // shared state/status protected by this strand
          ec_last_ctrl_tx,                // last error_code (updared vis streams_strand)
          lead_time                       // default lead_time
      );
    });
  }
}

void Desk::save_anchor_data(anchor::Data data) {
  auto anchor = Anchor::ptr().get();
  anchor->save(data);

  adjust_play_mode(data.play_mode());
}

// misc debug and logging API
void Desk::log_despooled(shFrame frame, Elapsed &elapsed) {
  static uint64_t no_playable = 0;
  static uint64_t playable = 0;

  asio::defer(io_ctx, [=, this]() {
    string msg;
    auto w = std::back_inserter(msg);

    if (frame) {
      no_playable = 0;
      playable++;
      if ((frame->sync_wait < 10ms) || ((playable % 1000) == 0)) {
        fmt::format_to(w, " seq_num={} sync_wait={} state={}", frame->seq_num,
                       pe_time::as_duration<Nanos, MillisFP>(frame->sync_wait),
                       Frame::stateVal(frame));
      }
    } else {
      playable = 0;
      no_playable++;
      if ((no_playable == 1) || ((no_playable % 100) == 0)) {
        fmt::format_to(w, " no playable frames");
      }
    }

    if (elapsed() >= 3ms) {
      fmt::format_to(w, " elapsed={:0.2}", elapsed.as<MillisFP>());
    }

    if (!msg.empty()) {
      __LOG0(LCOL01 "{}\n", moduleID(), "DESPOOLED", msg);
    }
  });
}

} // namespace pierre
