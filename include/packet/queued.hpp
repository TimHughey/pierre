
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

#include <arpa/inet.h>
#include <array>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <source_location>
#include <vector>

#include "packet/basic.hpp"

namespace pierre {
namespace packet {

class Queued {
private:
  using cond_var = std::condition_variable;
  using mutex = std::mutex;
  using src_loc = std::source_location;
  using lock_guard = std::lock_guard<mutex>;
  using unique_lock = std::unique_lock<mutex>;
  typedef std::queue<packet::Basic> PacketQueue;
  typedef const char *ccs;

public:
  typedef std::promise<bool> PacketPromise;
  typedef std::future<bool> PacketFuture;

public:
  static constexpr size_t PACKET_LEN_BYTES = sizeof(uint16_t);
  static constexpr size_t STD_PACKET_SIZE = 2048;

public:
  Queued() {}

  packet::Basic &buffer() { return packet; }

  packet::Basic nextPacket();

  // notify that data has been queued
  void storePacket(const size_t rx_bytes);

  // buffer to receive the packet length
  auto lengthBuffer() { return boost::asio::buffer(&packet_len, 2); }
  auto length() { return ntohs(packet_len); }

  void reset();

  bool waitForPacket();
  PacketFuture wantPacket();

private:
  inline auto lockQueue() { return unique_lock(qmtx); }

  // misc helpers
  ccs fnName(const src_loc loc = src_loc::current()) const { return loc.function_name(); }

private:
  PacketQueue packet_q; // queue of received packets
  packet::Basic packet; // packet received by Session

  std::optional<PacketPromise> promise;

  uint16_t packet_len; // populated by async_*

  mutex qmtx; // protect the queued data
  mutex pmtx; // protect the next packet promise

  bool packet_ready = false;
  mutex mtx_cv;             // protect the data ready cv
  cond_var cv_packet_ready; // sufficient data is available
};

} // namespace packet
} // namespace pierre