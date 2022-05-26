
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
#include "packet/rtp.hpp"

#include <boost/asio.hpp>
#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <semaphore>
#include <vector>

namespace pierre {

namespace packet {
class Queued;
typedef std::shared_ptr<Queued> shQueued;
} // namespace packet

namespace shared {
std::optional<packet::shQueued> &queued();
} // namespace shared

namespace packet {

namespace { // anonymous namespace limits scope
namespace asio = boost::asio;
}

class Queued : public std::enable_shared_from_this<Queued> {
public:
  using semaphore = std::counting_semaphore<1>;
  using nanos = std::chrono::nanoseconds;

  typedef std::map<uint32_t, RTP> PacketMap;
  typedef std::binary_semaphore QueueAccess;

public:
  static constexpr size_t PACKET_LEN_BYTES = sizeof(uint16_t);
  static constexpr size_t STD_PACKET_SIZE = 2048;

public:
  static shQueued init(asio::io_context &io_ctx) {
    return shared::queued().emplace(new Queued(io_ctx));
  }
  static shQueued ptr() { return shared::queued().value()->shared_from_this(); }
  static void reset() { shared::queued().reset(); }

private: // constructor private, all access through shared_ptr
  Queued(asio::io_context &io_ctx) : io_ctx(io_ctx) {}

public:
  inline Basic &buffer() { return _packet; }

  // notify that data has been queued
  void storePacket(const size_t rx_bytes);

  // buffer to receive the packet length
  auto lenBuffer() {
    packet_len.clear();

    return asio::dynamic_buffer(packet_len);
  }

  static constexpr uint16_t lenBytes = sizeof(uint16_t);
  uint16_t length();

  uint32_t seqFirst() const;
  uint32_t seqLast() const;
  size_t seqCount() const { return _packet_map.size(); }

  uint32_t timestFirst() const;
  uint32_t timestLast() const;

private:
  // order dependent (constructor initialized)
  asio::io_context &io_ctx;

  // order independent
  PacketMap _packet_map;      // queued packets mapped by seq num
  PacketMap _packet_map_busy; // holds queued packets when _packet_map is busy
  Basic _packet;              // packet received by Session
  Basic packet_len;           // buffer for packet len
  semaphore q_access{0};      // keep this, may need it

  static constexpr csv moduleId{"QUEUED"};
};

} // namespace packet
} // namespace pierre