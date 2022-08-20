/*
    Pierre - Custom Light Show via DMX for Wiss Landing
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

#include "render.hpp"
#include "base/elapsed.hpp"
#include "base/input_info.hpp"
#include "base/pe_time.hpp"
#include "base/typical.hpp"
#include "desk/desk.hpp"
#include "frame/frame.hpp"
#include "rtp_time/anchor.hpp"
#include "spooler.hpp"

#include <ArduinoJson.h>
#include <chrono>
#include <memory>
#include <optional>

namespace asio_errc = boost::asio::error;

namespace pierre {
namespace player {

Render::Render(io_context &io_ctx, Spooler &spooler)
    : io_ctx(io_ctx),                      // grab the io_ctx
      spooler(spooler),                    // use this spooler for unloading Frames
      local_strand(spooler.strandOut()),   // use the unload strand from spooler
      handle_timer(io_ctx),                //
      release_timer(io_ctx),               // handle available frames
      stats_timer(io_ctx),                 // period stats reporting
      lead_time(InputInfo::frame<Nanos>()) // frame lead time
{
  __LOG0(LCOL01 " lead_time={} fps={:0.3f}\n", moduleId, csv("CONSTRUCT"), lead_time,
         InputInfo::fps());

  Desk::init();
}

// public API and member functions
void Render::adjust_play_mode(csv mode) {
  asio::post(local_strand, [this, mode = mode]() {
    play_mode = mode;

    if (play_mode.front() == PLAYING.front()) {
      next_frame();
      report_stats();
    } else {
      handle_timer.cancel();
      release_timer.cancel();
      stats_timer.cancel();
    }
  });
}

void Render::next_frame(Nanos sync_wait, Nanos lag) {
  static csv fn_id = "NEXT_FRAME";

  handle_timer.expires_after(sync_wait);
  handle_timer.async_wait( // wait the required sync duration before next frame
      asio::bind_executor(local_strand, [=, this](const error_code &ec) mutable {
        Elapsed elapsed;

        if (!ec && playing()) {

          if (auto frame = spooler.nextFrame(lead_time); frame && frame->playable()) {
            sync_wait = frame->calc_sync_wait(lag);

            __LOGX(LCOL01 " seq_num={} sync_wait={}\n", moduleID(), fn_id, frame->seq_num,
                   pe_time::as_duration<Nanos, MillisFP>(sync_wait));

            // mark played and immediately send the frame to Desk
            frame->mark_played();
            Desk::next_frame(frame);
            frames_handled++;
          } else {
            __LOG0(LCOL01 " no playable frames\n", moduleID(), fn_id);
            sync_wait = Nanos(lead_time / 2);
            frames_none++;
          }

          if (elapsed() >= 3ms) {
            __LOG0(LCOL01 " elapsed={:0.2} sync_wait={}\n", moduleID(), fn_id,
                   elapsed.as<MillisFP>(), pe_time::as_duration<Nanos, MillisFP>(sync_wait));
          }

          // use the calculated sync_wait to control frame rate (when played) or
          // poll for the next frame using the default sync_wait
          next_frame(sync_wait, elapsed());
        } else {
          __LOG0(LCOL01 " playing={} reason={}\n", moduleID(), fn_id, playing(), ec.message());
        }
      }));
}

void Render::report_stats(const Nanos interval) {
  stats_timer.expires_after(interval);
  stats_timer.async_wait([this](const error_code ec) {
    if (!ec) {
      __LOG0(LCOL01 " frames handled={:>6} none={:>6}\n", moduleId, "STATS", frames_handled,
             frames_none);

      report_stats(STATS_INTERVAL);
    }
  });
}

} // namespace player
} // namespace pierre
