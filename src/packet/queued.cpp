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

#include <algorithm>
#include <boost/asio/buffer.hpp>
#include <chrono>
#include <fmt/format.h>
#include <iterator>

#include "core/input_info.hpp"
#include "packet/queued.hpp"
#include "packet/rfc3550/hdr.hpp"
#include "packet/rfc3550/trl.hpp"

using namespace std::chrono_literals;

namespace pierre {
namespace packet {

packet::Basic Queued::nextPacket() {
  unique_lock q_lck(qmtx);

  auto next_packet = packet::Basic(packet_q.front()); // get the next available packet
  packet_q.pop();                                     // pop the swapped packet from the queue

  unique_lock cv_lck(mtx_cv);
  packet_ready = !packet_q.empty();

  return next_packet;
}

void Queued::storePacket([[maybe_unused]] const size_t rx_bytes) {
  if (false) {
    [[maybe_unused]] auto hdr = rfc3550::hdr(packet);
    [[maybe_unused]] auto trl = rfc3550::trl(packet);
    hdr.dump();
  }

  // create the locks but don't lock them, yet
  unique_lock q_lck(qmtx, std::defer_lock);

  if (rfc3550::hdr(packet).isValid()) { // if the packet is valid
    q_lck.lock();

    packet_q.emplace(packet); // stuff it in the queue

    packet.clear();

    q_lck.unlock(); // done with queue

    unique_lock cv_lck(mtx_cv);
    packet_ready = true;
    cv_lck.unlock();

    cv_packet_ready.notify_all();
  }
}

void Queued::reset() {
  // maybe something later
}

bool Queued::waitForPacket() {
  if (!packet_ready) {
    unique_lock cv_lck(mtx_cv);

    cv_packet_ready.wait_for(cv_lck, 15s, [&] { return packet_ready; });
  }

  return packet_ready;
}

Queued::PacketFuture Queued::wantPacket() {
  unique_lock p_lck(pmtx);

  return promise.emplace(PacketPromise()).get_future();
}

// misc helpers, debug, etc.

} // namespace packet
} // namespace pierre
