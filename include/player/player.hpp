
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

#include "core/typedefs.hpp"
#include "packet/basic.hpp"
#include "player/flush_request.hpp"
#include "player/frames_request.hpp"
#include "player/spooler.hpp"
#include "rtp_time/anchor/data.hpp"

#include <boost/asio.hpp>
#include <chrono>
#include <memory>
#include <optional>
#include <vector>

namespace pierre {

namespace { // anonymous namespace limits scope
namespace asio = boost::asio;
}

class Player;
typedef std::shared_ptr<Player> shPlayer;

class Player : public std::enable_shared_from_this<Player> {
private:
  typedef std::vector<Thread> Threads;

private: // constructor private, all access through shared_ptr
  Player(asio::io_context &io_ctx);

public:
  static shPlayer init(asio::io_context &io_ctx);
  static shPlayer ptr();
  static void reset();

  static void accept(packet::Basic &packet_accept);
  static void flush(const FlushRequest &request);

  static void saveAnchor(anchor::Data &data);
  static void shutdown();
  static void teardown();

private:
  void run();
  void start();
  void watchDog();

private:
  // order dependent
  asio::io_context dsp_io_ctx;                // dsp work
  asio::io_context::strand local_strand;      // serialize player activites
  asio::io_context::strand decode_strand;     // serialize packet decoding
  asio::io_context::strand spooler_strand;    // serialize spooler activities
  asio::high_resolution_timer watchdog_timer; // watch for shutdown
  player::shSpooler spooler;
  player::shFramesRequest frames_request;

  // order independent
  Thread dsp_thread;
  Threads dsp_threads;

  uint64_t packet_count = 0;
  bool _is_playing = false;
  FlushRequest _flush;

  static constexpr csv moduleId{"PLAYER"};
};

} // namespace pierre