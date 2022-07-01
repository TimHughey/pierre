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

#pragma once

#include "base/pe_time.hpp"
#include "base/typical.hpp"
#include "io/io.hpp"
#include "player/frame.hpp"
#include "player/frame_time.hpp"
#include "player/spooler.hpp"
#include "player/typedefs.hpp"

#include <boost/asio.hpp>
#include <chrono>
#include <memory>
#include <optional>
#include <set>

namespace pierre {
namespace player {

namespace { // anonymous namespace limits scope to this file
using steady_clock = std::chrono::steady_clock;
} // namespace

class Render;
typedef std::shared_ptr<Render> shRender;

class Render : public std::enable_shared_from_this<Render> {
private:
  typedef steady_clock::time_point SteadyTimePoint;
  static constexpr FrameTimeDiff FTD{.old = pe_time::negative(dmx::frame_ns()),
                                     .late = pe_time::negative(Nanos(dmx::frame_ns() / 2)),
                                     .lead = dmx::frame_ns()};

private: // all access via shared_ptr
  Render(io_context &io_ctx, shSpooler spooler);

public: // static creation, access, etc.
  static shRender init(io_context &io_ctx, shSpooler spooler);
  static shRender ptr();
  static void reset();

public: // public API
  static void playMode(csv mode) {
    asio::post(ptr()->local_strand, [self = ptr(), mode = mode]() { self->playStart(mode); });
  }

  static void teardown() { ptr()->playMode(NOT_PLAYING); }

private:
  void frameTimer();
  void handleFrame();
  static void nextPacket(shFrame next_packet, const Nanos start);

  bool playing() const { return play_mode.front() == PLAYING.front(); }

  void playStart(csv mode) {
    play_mode = mode;

    if (play_mode.front() == PLAYING.front()) {
      render_start = steady_clock::now();
      rendered_frames = 0;
      frameTimer(); // frame timer handles play/not play
      statsTimer();
    } else {
      frame_timer.cancel();
      stats_timer.cancel();
      render_start = SteadyTimePoint();
    }
  }

  // misc debug, stats

  const string stats() const;
  void statsTimer(const Nanos report_ns = 10s);

private:
  // order dependent
  io_context &io_ctx;
  shSpooler spooler;
  strand &local_strand;
  steady_timer frame_timer;
  steady_timer stats_timer;
  steady_timer silence_timer;
  steady_timer leave_timer;
  steady_timer standby_timer;

  SteadyTimePoint render_start;
  uint64_t rendered_frames = 0;

  // order independent
  string_view play_mode = NOT_PLAYING;
  string_view fx_name;
  shFrame recent_frame;
  uint64_t frames_played = 0;
  uint64_t frames_silence = 0;

  static constexpr auto moduleId = csv("RENDER");
};
} // namespace player
} // namespace pierre
