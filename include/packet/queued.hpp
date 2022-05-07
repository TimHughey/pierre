
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
#include <cstdint>
#include <future>
#include <memory>
#include <queue>
#include <semaphore>
#include <source_location>
#include <vector>

#include "packet/basic.hpp"

namespace pierre {
namespace packet {

class Queued {
private:
  using src_loc = std::source_location;
  using semaphore = std::counting_semaphore<1>;
  using nanos = std::chrono::nanoseconds;

  typedef std::queue<packet::Basic> PacketQueue;
  typedef std::binary_semaphore QueueAccess;
  typedef std::promise<packet::Basic> PromisePacket;
  typedef std::future<PromisePacket> FuturePacket;
  typedef const char *ccs;
  typedef const src_loc csrc_loc;

public:
  static constexpr size_t PACKET_LEN_BYTES = sizeof(uint16_t);
  static constexpr size_t STD_PACKET_SIZE = 2048;

public:
  Queued() {}

  packet::Basic &buffer() {
    packet.clear(); // ensure the buffer is cleared
    return packet;
  }

  FuturePacket nextPacket();

  // notify that data has been queued
  void storePacket(const size_t rx_bytes);

  // buffer to receive the packet length
  auto lenBuffer() { return boost::asio::buffer(&packet_len, 2); }
  auto length() { return ntohs(packet_len); }

  void reset();

private:
  // misc helpers
  static ccs fnName(csrc_loc loc = src_loc::current()) { return loc.function_name(); }

private:
  PacketQueue packet_q; // queue of received packets
  packet::Basic packet; // packet received by Session
  PromisePacket next_packet;
  uint16_t packet_len; // populated by async_*

  semaphore q_access{0};
};

} // namespace packet
} // namespace pierre