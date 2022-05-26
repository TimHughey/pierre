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

#include "packet/queued.hpp"
#include "packet/rtp.hpp"

#include <algorithm>
#include <boost/asio/buffer.hpp>
#include <chrono>
#include <fmt/format.h>
#include <iterator>
#include <mutex>

using namespace std::chrono_literals;

namespace pierre {

namespace shared {
std::optional<packet::shQueued> __queued;
std::optional<packet::shQueued> &queued() { return __queued; }
} // namespace shared

namespace packet {

uint16_t Queued::length() {
  uint16_t len = 0;

  len += packet_len[0] << 8;
  len += packet_len[1];

  if (false) { // debug
    auto constexpr f = FMT_STRING("{} {} len={}\n");
    fmt::print(f, runTicks(), fnName(), len);
  }

  return len;
}

uint32_t Queued::seqFirst() const {
  if (_packet_map.size() > 0) {
    return _packet_map.begin()->first;
  }

  return 0;
}

uint32_t Queued::seqLast() const {
  if (_packet_map.size() > 0) {
    return _packet_map.rbegin()->first;
  }

  return 0;
}

uint32_t Queued::timestFirst() const {
  if (_packet_map.size() > 0) {
    return _packet_map.begin()->second.timestamp;
  }

  return 0;
}

uint32_t Queued::timestLast() const {
  if (_packet_map.size() > 0) {
    return _packet_map.rbegin()->second.timestamp;
  }

  return 0;
}

void Queued::storePacket([[maybe_unused]] const size_t rx_bytes) {
  static std::once_flag __once_flag;
  std::call_once(__once_flag, [&] { q_access.release(); }); // semaphore release to get started

  auto rtp_packet = RTP(_packet);
  uint32_t seq_num = rtp_packet.seq_num;

  if (false) { // debug
    constexpr auto f = FMT_STRING("{} QUEUED vsn={} pad={} ext={} ssrc_count={} "
                                  "payload_size={:>4} seq_num={:>8} ts={:>12} \n");
    fmt::print(f, runTicks(), rtp_packet.version, rtp_packet.padding, rtp_packet.extension,
               rtp_packet.ssrc_count, rtp_packet.payloadSize(), rtp_packet.seq_num,
               rtp_packet.timestamp);
  }

  if (rtp_packet.isValid() == false) { // discard invalid RTP packets
    if (true) {
      constexpr auto f = FMT_STRING("{} {} dropping invalid RTP packet\n");
      fmt::print(f, runTicks(), moduleId);
    }

    return;
  }

  if (rtp_packet.decipher() == false) { // discard decipher failures
    return;
  }

  // we have a valid RTP packet, store it somewhere

  if (q_access.try_acquire_for(10ms) == false) { // primary packet map busy, store in busy map
    if (true) {                                  // debug
      constexpr auto f = FMT_STRING("{} {} semaphore aquisition failed, "
                                    "saving in busy map size={}\n");
      fmt::print(f, runTicks(), moduleId, _packet_map_busy.size());
    }

    _packet_map_busy.insert_or_assign(seq_num, rtp_packet);
    return;
  }

  if (_packet_map_busy.size() > 0) { // move any busy packets to primary map
    for (auto &[seq_num, __rtp_packet_busy] : _packet_map_busy) {
      _packet_map.insert(_packet_map_busy.extract(seq_num));
    }
  }

  if (_packet_map.contains(seq_num)) {
    if (true) { // debug
      constexpr auto f = FMT_STRING("{} PACKET MAP seq={:>10} exists\n");
      fmt::print(f, runTicks(), seq_num);
    }
  }

  // store the received rtp_packet
  _packet_map.insert_or_assign(seq_num, rtp_packet);

  q_access.release(); // release the queue
}

// misc helpers, debug, etc.

} // namespace packet
} // namespace pierre
