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

Queued::DataFuture Queued::dataRequest(size_t bytes) {
  promises_mtx.lock();
  promise.emplace(DataPromise());
  promised_bytes = bytes;
  promises_mtx.unlock();

  return promise->get_future();
}

bool Queued::deque(Packet &buffer, size_t bytes) {
  buffer.clear(); // buffer is always cleared

  const std::lock_guard<std::mutex> __lock__(queue_mtx);
  if (queued.size() < bytes) {
    return false;
  }

  for (size_t i = 0; i < bytes; i++) {
    buffer.emplace_back(queued.back());
    queued.pop_back();
  }

  return true;
}

void Queued::gotBytes(const size_t rx_bytes) {
  queue_mtx.lock();

  for (auto &in : packet) {
    queued.emplace_front(in);
  }

  queue_mtx.unlock();

  _rx_bytes += rx_bytes; // track the bytes received (for fun)

  packet.clear(); // clear the packet for the next rx

  if (promised_bytes && (queued.size() >= promised_bytes)) {
    auto bytes = promised_bytes;
    promised_bytes = 0;

    promise->set_value(bytes);
  }
}

void Queued::reset() {
  // maybe something later
}

// misc helpers, debug, etc.

} // namespace packet
} // namespace pierre
