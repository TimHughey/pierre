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
#include "base/pe_time.hpp"
#include "base/typical.hpp"
#include "core/input_info.hpp"
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
      frame_timer(io_ctx),                // generate process frames
      stats_timer(io_ctx),                // period stats reporting
      silence_timer(io_ctx),              // track silence
      leave_timer(io_ctx),                // track when to leave
      standby_timer(io_ctx),              // track when to enter standby
      render_start(),                     // play not started yet
      rendered_frames(0)                  // no frames played yet
{
  const auto render_fps = pe_time::as_millis_fp(dmx::frame_ns());
  const auto input_fps = pe_time::as_millis_fp(InputInfo::fps_ns());
  const auto diff = render_fps - input_fps;

  __LOG0(LCOL01 " render_fps={:0.1} input_fps={:0.1} diff={:0.2}\n", moduleId, csv("CONSTRUCT"),
         render_fps, input_fps, diff);

  // call no functions here that use self
}

// static methods for creating, getting and resetting the shared instance
shRender Render::init(asio::io_context &io_ctx, shSpooler spooler) {
  auto self = shared::render().emplace(new Render(io_ctx, spooler));
  auto desk = Desk::create();

  return self;
}

shRender Render::ptr() { return shared::render().value()->shared_from_this(); }
void Render::reset() { shared::render().reset(); }

// public API and member functions

void Render::frameTimer() {
  // create a precise frame_ns using the timepoint of when rendering started
  // and frames rendered
  frame_timer.expires_at(render_start + Nanos(dmx::frame_ns() * rendered_frames));

  frame_timer.async_wait(  // wait to generate next frame
      asio::bind_executor( // use a specific executor
          local_strand,    // serialize rendering
          [self = shared_from_this()](const error_code ec) {
            if (ec == errc::success) {
              self->handleFrame();

            } else if (ec != asio_errc::operation_aborted) {
              __LOG0(LCOL01 " terminating reason={}\n", self->moduleId, csv("FRAME TIMER"),
                     ec.message());
            }
          }));
}

void Render::handleFrame() {
  static csv CATEGORY{"FRAME TIMER"};
  [[maybe_unused]] Elapsed elapsed;

  ++rendered_frames;
  if (play_mode == PLAYING) {
    // Spooler::nextFrame() may return an empty frame

    auto frame = spooler->nextFrame(FTD);
    recent_frame = Frame::ensureFrame(frame)->markPlayed(frames_played, frames_silence);
    shFX active_fx = Desk::activeFX();
    fx_name = active_fx->name();

    if ((fx_name == fx::SILENCE) && !recent_frame->silence()) {
      Desk::activateFX<fx::MajorPeak>(fx::MAJOR_PEAK);
    }

    active_fx->executeLoop(recent_frame->peaksLeft());

    if (Frame::ok(frame) && frame->unplayed()) {
      __LOGX(LCOL01 " {}\n", moduleId, CATEGORY, Frame::inspectFrame(frame));
    }
  }

  frameTimer();

  if (elapsed.freeze() >= 3ms) {
    __LOG0(LCOL01 " elapsed={:0.2}\n", moduleId, CATEGORY, elapsed.as<MillisFP>());
  }
}

const string Render::stats() const {
  float silence = 1;
  if (frames_played) {
    silence = (float)frames_silence / (frames_played + frames_silence);
    silence = (silence > 0.0) ? silence : 1;
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
            if (ec != errc::success) {
              return;
            }

            __LOG0(LCOL01 " {}\n", moduleId, Frame::stateVal(self->recent_frame), self->stats());

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
