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
#include "base/pe_time.hpp"
#include "core/input_info.hpp"
#include "desk/desk.hpp"
#include "rtp_time/anchor.hpp"
#include "spooler.hpp"
#include "typedefs.hpp"

#include <ArduinoJson.h>
#include <chrono>
#include <memory>
#include <optional>

namespace pierre {
namespace shared {
std::optional<player::shRender> __render;
std::optional<player::shRender> &render() { return __render; }
} // namespace shared

namespace player {

namespace asio = boost::asio;
namespace errc = boost::system::errc;
using io_context = boost::asio::io_context;
using error_code = boost::system::error_code;
namespace chrono = std::chrono;

Render::Render(io_context &io_ctx, player::shSpooler spooler)
    : io_ctx(io_ctx),                     // grab the io_ctx
      spooler(spooler),                   // use this spooler for unloading Frames
      local_strand(spooler->strandOut()), // use the unload strand from spooler
      frame_timer(io_ctx),                // generate process frames
      stats_timer(io_ctx),                // period stats reporting
      silence_timer(io_ctx),              // track silence
      leave_timer(io_ctx),                // track when to leave
      standby_timer(io_ctx),              // track when to enter standby
      play_start(SteadyTimePoint::min()), // play not started yet
      play_frame_counter(0)               // no frames played yet
{
  const auto render_fps = pe_time::as_millis_fp(dmx::frame_ns());
  const auto input_fps = pe_time::as_millis_fp(InputInfo::fps_ns());
  const auto diff = render_fps - input_fps;

  __LOG0("{:<18} render_fps={} input_fps={} diff={}\n", moduleId, render_fps, input_fps, diff);

  // call no functions here that use self
}

// static methods for creating, getting and resetting the shared instance
shRender Render::init(asio::io_context &io_ctx, player::shSpooler spooler) {
  auto self = shared::render().emplace(new Render(io_ctx, spooler));

  auto desk = Desk::create();

  return self;
}

shRender Render::ptr() { return shared::render().value()->shared_from_this(); }
void Render::reset() { shared::render().reset(); }

// public API and member functions

void Render::frameTimer() {
  // create a precise frame_ns using the elapsed time of last frame
  frame_timer.expires_at(play_start + Nanos(dmx::frame_ns() * play_frame_counter));

  frame_timer.async_wait(  // wait to generate next frame
      asio::bind_executor( // use a specific executor
          local_strand,    // serialize rendering
          [self = shared_from_this()](const error_code ec) {
            if (ec == errc::success) {
              self->handleFrame();

            } else {
              __LOG0("{:<18} terminating reason={}\n", csv("FRAME TIMER"), ec.message());
            }
          }));
}

void Render::handleFrame() {
  ++play_frame_counter;

  if (play_mode == player::PLAYING) {
    // Spooler::nextFrame() may return an empty frame

    auto frame = spooler->nextFrame(FTD);

    recent_frame = player::Frame::markPlayed(frame, frames_played, frames_silence);

    if (player::Frame::ok(frame)) {
      auto peaks = frame->peaksLeft();

      if (Peaks::silence(peaks)) {
      }
    }

    if (player::Frame::ok(frame) && frame->unplayed()) {
      __LOGX("{:<18} FRAME {}\n", moduleId, player::Frame::inspectFrame(frame));
    }

    frameTimer();
  }
}

const string Render::stats() const {
  float silence = 1;
  if (frames_played) {
    silence = (float)frames_silence / (frames_played + frames_silence);
    silence = (silence > 0.0) ? silence : 1;
  }

  return fmt::format("state={:<10} played={:<8} silent={:<8} silence={:6.2f}%", //
                     player::Frame::stateVal(recent_frame), frames_played, frames_silence,
                     silence * 100.0);
}

void Render::statsTimer(const Nanos report_ns) {
  // should the frame timer run?
  if (play_mode == player::NOT_PLAYING) {
    [[maybe_unused]] error_code ec;
    stats_timer.cancel(ec);
    return;
  }

  stats_timer.expires_after(report_ns);

  stats_timer.async_wait(  // wait to report stats
      asio::bind_executor( // use a specific executor
          local_strand,    // use our local strand
          [self = shared_from_this()](const error_code ec) {
            if (ec != errc::success) {
              return;
            }

            __LOG0("{:<18} {}\n", moduleId, self->stats());

            self->statsTimer();
          })

  );
}

/*
void Render::stream() {
  string func(__PRETTY_FUNCTION__);

  // this_thread::sleep_for(chrono::seconds(2));

  uint64_t frames = 0;
  long frame_us = ((1000 * 1000) / 44) - 250;
  duration frame_interval_half = us(frame_us / 2);

  while (State::isRunning()) {
    auto loop_start = steady_clock::now();
    for (auto p : producers) {
      p->prepare();
    }

    this_thread::sleep_for(frame_interval_half);

    packet::DMX packet;

    for (auto p : producers) {
      p->update(packet);
    }

    auto elapsed = duration_cast<us>(steady_clock::now() - loop_start);

    this_thread::sleep_for(us(frame_us) - elapsed);

    if (!State::isSuspended()) {
      _net.write(packet);
    }

    frames++;
  }
}
*/
} // namespace player

} // namespace pierre
