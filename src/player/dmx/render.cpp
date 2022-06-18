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

#include "dmx/render.hpp"
#include "core/input_info.hpp"
#include "player/frames_request.hpp"
#include "player/typedefs.hpp"
#include "rtp_time/anchor.hpp"
#include "rtp_time/rtp_time.hpp"
#include "spooler.hpp"

#include <ArduinoJson.h>
#include <chrono>
#include <memory>
#include <optional>

typedef StaticJsonDocument<1024> Document;

namespace pierre {
namespace shared {
std::optional<dmx::shRender> __render;
std::optional<dmx::shRender> &render() { return __render; }
} // namespace shared

namespace dmx {

namespace asio = boost::asio;
namespace errc = boost::system::errc;
using io_context = boost::asio::io_context;
using error_code = boost::system::error_code;
namespace chrono = std::chrono;

Render::Render(io_context &io_ctx, player::shFramesRequest frames_request)
    : local_strand(io_ctx),                               // create our local strand
      frame_timer(io_ctx),                                // generate DMX frames
      stats_timer(io_ctx),                                // period stats reporting
      spooler(player::Spooler::create(io_ctx, moduleId)), // create our local spool
      frames_request(frames_request) {
  const auto render_fps = chrono::duration_cast<MillisFP>(fps_ns());
  const auto input_fps = chrono::duration_cast<MillisFP>(InputInfo::fps_ns());
  const auto diff = render_fps - input_fps;

  __LOG0("render_fps={} input_fps={} diff={}\n", render_fps, input_fps, diff);

  frames_request->dest = spooler;
  // call no functions here that use self
}

// static methods for creating, getting and resetting the shared instance
shRender Render::init(asio::io_context &io_ctx, player::shFramesRequest frames_request) {
  auto self = shared::render().emplace(new Render(io_ctx, frames_request));

  return self;
}

shRender Render::ptr() { return shared::render().value()->shared_from_this(); }
void Render::reset() { shared::render().reset(); }

// public API and member functions
void Render::flush(const FlushRequest &request) { // static
  ptr()->spooler->flush(request);
}

void Render::frameTimer() {
  static Nanos last_timer_ns{0};

  if (playing() && (spooler->frameCount() < 64)) {
    frames_request->atLeastFrames(64);
  }

  const auto latency_ns = rtp_time::nowNanos() - last_timer_ns;
  const auto expires_ns = (latency_ns <= fps_ns()) ? fps_ns() - latency_ns : fps_ns();
  frame_timer.expires_after(expires_ns);

  frame_timer.async_wait(  // wait to generate next frame
      asio::bind_executor( // use a specific executor
          local_strand,    // serialize rendering
          [self = shared_from_this()](const error_code ec) {
            last_timer_ns = rtp_time::nowNanos(); // used to calculate next timer

            if (ec != errc::success) {
              __LOG0("FRAME TIMER terminating reason={}\n", ec.message());
              return;
            }

            if (MasterClock::ptr()->ok()) {
              if (self->playing()) {
                self->handleFrame();
              }
            }

            self->frameTimer();
          }));
}

shRender Render::handleFrame() {
  if (_play_mode == player::PLAYING) {
    auto frame = spooler->nextFrame(fps_ns());

    if (player::RTP::playable(frame)) {
      frame->markPlayed();
      _frames_played += 1;
    } else {
      _frames_silence += 1;
    }

    _recent_frame = frame;
  }

  return shared_from_this();
}

void Render::playMode(bool mode) { // static
  auto self = ptr();

  asio::post(self->local_strand, // sync to frame timer
             [mode = mode, self = self]() mutable {
               self->_play_mode = mode; // save the play mode

               self->frameTimer(); // frame timer handles play/not play
               self->statsTimer();
             });
}

const string Render::stats() const {
  const float silenced = ((float)_frames_silence / float(_frames_played)) * 100.0;

  return fmt::format("{:18} state={:<10} played={:<8} silence={:<8} silenced={:0.3g}", //
                     moduleId, player::RTP::stateVal(_recent_frame), _frames_played,
                     _frames_silence, silenced);
}

void Render::statsTimer(const Nanos report_ns) {
  // should the frame timer run?
  if (_play_mode == player::NOT_PLAYING) {
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

            __LOG0("{}\n", self->stats());

            self->statsTimer();
          })

  );
}

void Render::teardown() { //.static
  auto self = ptr();

  [[maybe_unused]] error_code ec;
  self->frame_timer.cancel(ec);

  self->_play_mode = player::NOT_PLAYING;
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
    for (auto p : _producers) {
      p->prepare();
    }

    this_thread::sleep_for(frame_interval_half);

    dmx::Packet packet;

    for (auto p : _producers) {
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
} // namespace dmx

} // namespace pierre
