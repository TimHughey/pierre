
//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2022  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com
//
//  This work based on and inspired by
//  https://github.com/mikebrady/nqptp Copyright (c) 2021--2022 Mike Brady.

#pragma once

#include "base/flush_request.hpp"
#include "base/threads.hpp"
#include "base/typical.hpp"
#include "base/uint8v.hpp"
#include "io/io.hpp"
#include "player/spooler.hpp"
#include "player/stats.hpp"
#include "rtp_time/anchor/data.hpp"

#include <chrono>
#include <memory>
#include <optional>
#include <stop_token>
#include <vector>

namespace pierre {

class Player;
typedef std::shared_ptr<Player> shPlayer;

class Player : public std::enable_shared_from_this<Player> {
private:
  static constexpr auto PLAYER_THREADS = 3; // +1 include main thread

private: // constructor private, all access through shared_ptr
  Player(const Nanos &lead_time)
      : spooler(io_ctx),                   // local automatic object
        lead_time(lead_time),              // default frame lead time
        frame_strand(spooler.strandOut()), // next frame serialized processing
        frame_timer(io_ctx),               // timer for next frame sync
        stats(io_ctx, 10s),                // stats reporter (uses Player executor)
        watchdog_timer(io_ctx_dsp),        // watches for shutdown requests
        guard(io_ctx.get_executor())       // maybe not needed
  {
    // call no member functions that use shared_from_this() in constructor
  }

public:
  static void init(const Nanos &lead_time);
  static shPlayer ptr();
  static void reset();

  static void accept(uint8v &packet);

  void adjust_play_mode(csv next_mode) {
    play_mode = next_mode;

    if (play_mode == PLAYING) {
      next_frame();
      stats.async_report(5ms);
    } else {
      frame_timer.cancel();
      stats.cancel();
    }
  }

  static void flush(const FlushRequest &request) { ptr()->spooler.flush(request); }

  static void saveAnchor(anchor::Data &data); // must be declared in .cpp
  static void teardown();                     // must be declared in .cpp

  // misc debug
  static csv moduleID() { return module_id; }

private:
  void next_frame(Nanos sync_wait = 1ms, Nanos lag = 1ms);
  bool playing() const { return play_mode.front() == PLAYING.front(); }

  void stop_io() {
    guard.reset();
    io_ctx.stop();
    io_ctx_dsp.stop();
  }

  void watch_dog();

private:
  // order dependent
  Elapsed uptime;              // runtime of this object
  io_context io_ctx;           // player (and friends) context
  io_context io_ctx_dsp;       // dsp work
  player::Spooler spooler;     // in/out spooler
  Nanos lead_time;             // frame lead time
  strand frame_strand;         // next frame serialized processing
  steady_timer frame_timer;    // timer for next frame sync
  player::stats stats;         // stats reporter
  steady_timer watchdog_timer; // watch for shutdown
  work_guard guard;

  // order independent
  Thread thread_main;
  Threads threads;
  std::stop_token stop_token;

  string_view play_mode = NOT_PLAYING;
  FlushRequest flush_request;

  static constexpr csv module_id = "PLAYER";
};

} // namespace pierre