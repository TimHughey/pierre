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

#pragma once

#include "common/ss_inject.hpp"
#include "conn_info/conn_info.hpp"
#include "packet/queued.hpp"

#include <fmt/format.h>
#include <memory>
#include <optional>

namespace pierre {
namespace airplay {
namespace session {

class Audio; // forward decl for shared_ptr def
typedef std::shared_ptr<Audio> shAudio;

class Audio : public std::enable_shared_from_this<Audio> {
public:
  enum Accumulate { RX, TX };

public:
  ~Audio();

public:
  static void start(const Inject &di) {
    // creates the shared_ptr and starts the async loop
    // the asyncLoop holds onto the shared_ptr until an error on the
    // socket is detected
    shAudio(new Audio(di))->asyncLoop();
  }

private:
  Audio(const Inject &di)
      : socket(std::move(di.socket)),     // newly opened socket for session
        wire(ConnInfo::ptr()->raw_queued) // where to put packets
  {}

public:
  // initiates async audio buffer loop
  void asyncLoop(); // see .cpp file for critical function details
  void teardown();

private:
  void asyncReportRxBytes(int64_t rx_bytes = 0);
  void asyncRxPacket(size_t packet_len);

  bool isReady() const { return socket.is_open(); };
  bool isReady(const error_code &ec, csrc_loc loc = src_loc::current());

  void prepNextPacket() noexcept; // prepare for next buffer

  // misc stats and debug
  void accumulate(Accumulate type, size_t bytes);

private:
  // order dependent - initialized by constructor
  tcp_socket socket;
  packet::Queued &wire; // this is a reference into conn

  int64_t _rx_bytes = 0; // we do signed calculations
  int64_t _tx_bytes = 0;

  std::optional<high_res_timer> timer;

  static constexpr size_t STD_PACKET_SIZE = 2048;
};

} // namespace session
} // namespace airplay
} // namespace pierre