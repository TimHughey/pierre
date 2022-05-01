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
#include <fmt/format.h>
#include <iterator>

#include "core/input_info.hpp"
#include "packet/queued.hpp"

namespace pierre {
namespace packet {

Queued::Queued(size_t packet_size) : packet_size(packet_size) {}

void Queued::gotBytes(const size_t rx_bytes) {
  queue_mtx.lock();
  queued.insert(queued.end(), packet.begin(), packet.end());
  queue_mtx.unlock();

  _rx_bytes += rx_bytes; // track the bytes received (for fun)

  packet.clear(); // clear the packet for the next rx

  if (queued.size() > InputInfo::pcmBufferSize()) {
    data_ready_mtx.lock();

    data_ready_cv.notify_all();

    data_ready_mtx.unlock();
  }
}

void Queued::reset() {
  // maybe something later
}

// misc helpers, debug, etc.

} // namespace packet
} // namespace pierre
