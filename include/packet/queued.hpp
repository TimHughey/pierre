
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

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <source_location>
#include <vector>

namespace pierre {
namespace packet {

class Queued {
public:
  typedef std::vector<uint8_t> Packet;

private:
  using cond_var = std::condition_variable;
  using mutex = std::mutex;
  using src_loc = std::source_location;
  typedef std::deque<uint8_t> QueuedData;
  typedef const char *ccs;

public:
  typedef std::promise<size_t> DataPromise;
  typedef std::future<size_t> DataFuture;

public:
  Queued(size_t packet_size = STD_PACKET_SIZE);

  Packet &buffer() { return packet; }

  DataFuture dataRequest(size_t bytes);

  [[nodiscard]] bool deque(Packet &buffer, size_t bytes);

  // notify that data has been queued
  void gotBytes(const size_t rx_bytes);

  auto nominal() const { return packet_size / 8; }

  auto readyBytes() const { return queued.size(); }

  void reset();

private:
  auto lockQueue() { return std::unique_lock<mutex>(queue_mtx); }

  // misc helpers
  ccs fnName(const src_loc loc = src_loc::current()) const { return loc.function_name(); }

private:
  QueuedData queued;
  Packet packet;

  std::optional<DataPromise> promise;
  size_t promised_bytes = 0;

  const size_t packet_size;

  mutex queue_mtx;    // protect the queued data
  mutex promises_mtx; // protect the promises and data_requests

  mutex data_ready_mtx;   // protect the data ready cv
  cond_var data_ready_cv; // sufficient data is available

  size_t _rx_bytes = 0;

  static constexpr size_t STD_PACKET_SIZE = 2048;
};

} // namespace packet
} // namespace pierre