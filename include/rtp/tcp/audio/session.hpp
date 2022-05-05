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

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <fmt/format.h>
#include <memory>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

#include "core/input_info.hpp"
#include "packet/queued.hpp"

namespace pierre {
namespace rtp {
namespace tcp {

class AudioSession; // forward decl for shared_ptr def
typedef std::shared_ptr<AudioSession> sAudioSession;

class AudioSession : public std::enable_shared_from_this<AudioSession> {
public:
  enum Accumulate { RX, TX };

public:
  using io_context = boost::asio::io_context;
  using high_res_timer = boost::asio::high_resolution_timer;
  using error_code = boost::system::error_code;
  using tcp_socket = boost::asio::ip::tcp::socket;
  using string = std::string;
  using string_view = std::string_view;
  using src_loc = std::source_location;
  typedef const char *ccs;

public:
  struct Opts {
    tcp_socket &&new_socket;
    packet::Queued &audio_raw;
  };

public:
  ~AudioSession() { teardown(); }

public:
  [[nodiscard]] static sAudioSession create(const Opts &opts) {
    return sAudioSession(new AudioSession(opts));
  }

  sAudioSession getSelf() { return shared_from_this(); }

private:
  // AudioSession(tcp_socket &&socket);
  AudioSession(const Opts &opts);

public:
  // initiates async audio buffer loop
  void asyncAudioBufferLoop(); // see .cpp file for critical function details
  void teardown();

private:
  void asyncReportRxBytes(int64_t rx_bytes = 0);
  void asyncRxPacket(size_t packet_len);

  bool isReady() const { return socket.is_open(); };
  bool isReady(const error_code &ec, const src_loc loc = src_loc::current());

  void nextAudioBuffer(); // prepare for next buffer

  // bool rxAtLeast(size_t bytes = 1);
  // bool rxAvailable(); // load bytes immediately available

  // misc stats and debug
  void accumulate(Accumulate type, size_t bytes);
  static ccs fnName(const src_loc loc = src_loc::current()) { return loc.function_name(); }

private:
  // order dependent - initialized by constructor
  tcp_socket socket;
  packet::Queued &wire;

  int64_t _rx_bytes = 0; // we do signed calculations
  int64_t _tx_bytes = 0;

  std::optional<high_res_timer> timer;

  bool _shutdown = false;

  static constexpr size_t STD_PACKET_SIZE = 2048;
};

} // namespace tcp
} // namespace rtp
} // namespace pierre