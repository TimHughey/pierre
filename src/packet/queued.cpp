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

Queued::FuturePacket Queued::nextPacket() {
  PromisePacket _new_promise;

  std::swap(next_packet, _new_promise);

  return next_packet.get_future();
}

void Queued::storePacket([[maybe_unused]] const size_t rx_bytes) {
  static std::once_flag __once_flag;

  auto hdr = rfc3550::hdr(packet);

  if (false) { // debug
    [[maybe_unused]] auto trl = rfc3550::trl(packet);
    hdr.dump();
  }

  if (rfc3550::hdr(packet).isValid()) {                       // store packet if valid
    std::call_once(__once_flag, [&] { q_access.release(); }); //
    auto granted = q_access.try_acquire_for(10ms);

    if (!granted) { // drop packet
      constexpr auto f = FMT_STRING("{} semaphore aquisition failed, dropping packet size={}\n");
      fmt::print(f, fnName(), packet.size());
      return;
    }

    packet_q.emplace(packet); // store the packet
    q_access.release();       // release the queue
  }
}

void Queued::reset() {
  // maybe something later
}

// misc helpers, debug, etc.

} // namespace packet
} // namespace pierre
