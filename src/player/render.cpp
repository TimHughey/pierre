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
#include "desk/fx.hpp"
#include "desk/fx/majorpeak.hpp"
#include "desk/fx/names.hpp"
#include "desk/fx/silence.hpp"
#include "rtp_time/anchor.hpp"
#include "spooler.hpp"

#include <ArduinoJson.h>
#include <chrono>
#include <memory>
#include <optional>

namespace asio_errc = boost::asio::error;

namespace pierre {
namespace shared {
std::optional<player::shRender> __render;
std::optional<player::shRender> &render() { return __render; }
} // namespace shared

namespace player {
namespace chrono = std::chrono;

using namespace std::literals::chrono_literals;

Render::Render(io_context &io_ctx, shSpooler spooler)
    : io_ctx(io_ctx),                     // grab the io_ctx
      spooler(spooler),                   // use this spooler for unloading Frames
      local_strand(spooler->strandOut()), // use the unload strand from spooler
      handle_timer(io_ctx),               //
      release_timer(io_ctx),              // handle available frames
      stats_timer(io_ctx),                // period stats reporting
      lead_time(InputInfo::fps_ns())      // frame lead time
{
  __LOG0(LCOL01 " lead_time_ms={:0.1}\n", moduleId, csv("CONSTRUCT"),
         pe_time::as_millis_fp(lead_time));

  // call no functions here that use self
}

// static methods for creating, getting and resetting the shared instance
shRender Render::init(asio::io_context &io_ctx, shSpooler spooler) {
  auto self = shared::render().emplace(new Render(io_ctx, spooler));
  auto desk = Desk::create(io_ctx);

  return self;
}

shRender Render::ptr() { return shared::render().value()->shared_from_this(); }
void Render::reset() { shared::render().reset(); }

// public API and member functions

void Render::handle_frames() {
  static csv fn_id = "HANDLE_FRAMES";
  [[maybe_unused]] Elapsed elapsed;

  auto sync_wait = lead_time;

  if (auto frame = spooler->nextFrame(lead_time); frame && frame->playable()) {
    sync_wait = frame->syncWaitDuration(lead_time);

    __LOGX(LCOL01 " seq_num={} sync_wait={}\n", moduleID(), fn_id, frame->seq_num, sync_wait);
    schedule_release(frame, sync_wait);

  } else {
    __LOG0(LCOL01 " no playable frames\n", moduleID(), fn_id);
    handle_timer.expires_after(Nanos(lead_time / 3));
    handle_timer.async_wait(                                //
        [self = shared_from_this()](const error_code &ec) { //
          if (!ec) {
            self->post_handle_frames();
          }

        });
  }

  if (elapsed.freeze() >= 3ms) {
    __LOG0(LCOL01 " elapsed={:0.2}\n", moduleId, fn_id, elapsed.as<MillisFP>());
  }
}

// must be in .cpp due to intentional limited Desk visibility
void Render::release(shFrame frame) {
  Desk::next_frame(frame);
  frame->markPlayed(frames_played, frames_silence);

  post_handle_frames();
}

const string Render::stats() const {
  float silence = 0;
  if (frames_played && frames_silence) {
    silence = (float)frames_silence / (frames_played + frames_silence);
  } else if (frames_silence) {
    silence = 1.0;
  }

  string msg;
  auto w = std::back_inserter(msg);

  fmt::format_to(w, "played={:<8} silent={:<8} ", frames_played, frames_silence);
  fmt::format_to(w, "silence={:6.2f}%", silence * 100.0);

  return msg;
}

void Render::statsTimer(const Nanos report_ns) {
  // should the frame timer run?
  if (play_mode == NOT_PLAYING) {
    [[maybe_unused]] error_code ec;
    stats_timer.cancel(ec);
    return;
  }

  stats_timer.expires_after(report_ns);

  stats_timer.async_wait(  // wait to report stats
      asio::bind_executor( // use a specific executor
          local_strand,    // use our local strand
          [self = shared_from_this()](const error_code ec) {
            if (!ec) {
              __LOG0(LCOL01 " {}\n", moduleId, "STATS", self->stats());

              self->statsTimer();
            }
          })

  );
}

} // namespace player
} // namespace pierre
