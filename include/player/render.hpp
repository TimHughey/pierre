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

#include "base/elapsed.hpp"
#include "base/input_info.hpp"
#include "base/pe_time.hpp"
#include "base/typical.hpp"
#include "frame/frame.hpp"
#include "io/io.hpp"
#include "player/spooler.hpp"

#include <memory>
#include <optional>
#include <set>

namespace pierre {
namespace player {

class Render;
typedef std::shared_ptr<Render> shRender;

class Render : public std::enable_shared_from_this<Render> {
private: // all access via shared_ptr
  Render(io_context &io_ctx, shSpooler spooler);

public: // static creation, access, etc.
  static shRender init(io_context &io_ctx, shSpooler spooler);
  static shRender ptr();
  static void reset();

public: // public API
  static constexpr csv moduleID() { return moduleId; }
  static void playMode(csv mode) {
    auto self = ptr();
    asio::post(self->local_strand, [=]() { self->playStart(mode); });
  }

  static void teardown() { ptr()->playMode(NOT_PLAYING); }

private:
  void handle_frames();

  static void nextPacket(shFrame next_packet, const Nanos start);

  bool playing() const { return play_mode.front() == PLAYING.front(); }

  void playStart(csv mode) {
    asio::post(local_strand, [mode = mode, self = shared_from_this()] {
      self->play_mode = mode;

      if (self->play_mode.front() == PLAYING.front()) {

        self->wait_next_frame();
        self->statsTimer();
      } else {

        self->handle_timer.cancel();
        self->release_timer.cancel();
        self->stats_timer.cancel();
      }
    });
  }

  void wait_next_frame(Nanos sync_wait = Nanos(1ms)) {
    if (playing()) {

      handle_timer.expires_after(sync_wait);
      handle_timer.async_wait(asio::bind_executor( //
          local_strand,                            //
          [self = shared_from_this()](const error_code &ec) {
            if (!ec) {
              self->handle_frames();
            }
          }));
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
  steady_timer handle_timer;
  steady_timer release_timer;
  steady_timer stats_timer;

  Nanos lead_time;

  // order independent
  string_view play_mode = NOT_PLAYING;
  uint64_t frames_played = 0;
  uint64_t frames_silence = 0;
  Elapsed uptime;

  static constexpr auto moduleId = csv("RENDER");
};
} // namespace player
} // namespace pierre
