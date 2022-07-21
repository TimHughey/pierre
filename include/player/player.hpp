
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

#include "base/typical.hpp"
#include "base/uint8v.hpp"
#include "io/io.hpp"
#include "player/flush_request.hpp"
#include "player/spooler.hpp"
#include "rtp_time/anchor/data.hpp"

#include <boost/asio.hpp>
#include <chrono>
#include <memory>
#include <optional>
#include <vector>

namespace pierre {

class Player;
typedef std::shared_ptr<Player> shPlayer;

class Player : public std::enable_shared_from_this<Player> {
private:
  typedef std::vector<Thread> Threads;

private: // constructor private, all access through shared_ptr
  Player(io_context &io_ctx)
      : local_strand(io_ctx),                     // player serialized work
        decode_strand(io_ctx),                    // decoder serialized work
        spooler(player::Spooler::create(io_ctx)), // in/out spooler
        watchdog_timer(io_ctx_dsp) {              // dsp threads shutdown
    // call no member functions that use shared_from_this() in constructor
  }

public:
  static shPlayer init(io_context &io_ctx);
  static shPlayer ptr();
  static void reset();

  static void accept(uint8v &packet_accept);
  static void flush(const FlushRequest &request) { ptr()->spooler->flush(request); }
  static csv moduleID() { return module_id; }
  static void saveAnchor(anchor::Data &data); // must be declared in .cpp
  static void shutdown();
  static void teardown(); // must be declared in .cpp

private:
  shPlayer start();
  void watchDog();

private:
  // order dependent
  io_context io_ctx_dsp;       // dsp work
  strand local_strand;         // serialize player activites
  strand decode_strand;        // serialize packet decoding
  player::shSpooler spooler;   // in/out spooler
  steady_timer watchdog_timer; // watch for shutdown

  // order independent
  Threads dsp_threads;

  string_view play_mode = NOT_PLAYING;
  FlushRequest flush_request;
  uint64_t packet_count = 0;

  static constexpr csv module_id = "PLAYER";
};

} // namespace pierre