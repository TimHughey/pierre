
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
#include "packet/flush_request.hpp"
#include "packet/rtp.hpp"

#include <boost/asio.hpp>
#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <semaphore>

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
private:
  // a spool represets RTP packets, in ascending order by sequence
  // a spool is guaranteed gapless and all sequence numbers are ascending
  typedef std::deque<shRTP> Spool;

  // when a sequence number is received that is less than the previous
  // sequence number a new spool is created and added to spools. spools
  // will contain at least one spool at all times during playback.
  // spools contains more than one spool when sequence number rollover
  // occurs
  typedef std::deque<Spool> Spools;

public:
  static constexpr size_t PACKET_LEN_BYTES = sizeof(uint16_t);

public:
  static shQueued init(asio::io_context &io_ctx);
  static shQueued ptr();
  static void reset(); //

private: // constructor private, all access through shared_ptr
  Queued(asio::io_context &io_ctx);

public:
  void accept(Basic &&packet);
  inline Basic &buffer() { return _packet; }

  void flush(const FlushRequest &flush);
  void handoff(const size_t rx_bytes);

  // buffer to receive the packet length
  auto lenBuffer() {
    _packet_len.clear();

    return asio::dynamic_buffer(_packet_len);
  }

  static constexpr uint16_t lenBytes = sizeof(uint16_t);
  uint16_t length();

  int64_t packetCount() const;

  void teardown();

private:
  void stats();

private:
  // order dependent (constructor initialized)
  asio::io_context &io_ctx;
  asio::io_context::strand local_strand;
  asio::high_resolution_timer stats_timer;

  // order independent
  Basic _packet;     // packet received by Session
  Basic _packet_len; // buffer for packet len

  // one or more spools where front = earliest, back = latest
  Spools _spools;
  FlushRequest _flush;

  static constexpr csv moduleId{"QUEUED"};
};

} // namespace packet
} // namespace pierre