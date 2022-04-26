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
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

#include "packet/in.hpp"

namespace pierre {
namespace rtp {
namespace audio {
namespace buffered {

class Session; // forward decl for shared_ptr def
typedef std::shared_ptr<Session> sSession;

class Session : public std::enable_shared_from_this<Session> {
public:
  enum DumpKind { RawOnly, HeadersOnly, packet::ContentOnly };
  enum Accumulate { RX, TX };

public:
  using error_code = boost::system::error_code;
  using tcp_socket = boost::asio::ip::tcp::socket;
  using string = std::string;
  using string_view = std::string_view;
  using src_loc = std::source_location;
  typedef const char *ccs;

public:
  ~Session();
  // shared_ptr API
  [[nodiscard]] static sSession create(tcp_socket &&new_socket) {
    // must call constructor directly since it's private
    return sSession(new Session(std::forward<tcp_socket>(new_socket)));
  }

  sSession getSelf() { return shared_from_this(); }

private:
  Session(tcp_socket &&socket);

public:
  // initiates async audio buffer loop
  void asyncAudioBufferLoop(); // see .cpp file for critical function details

  void dump(DumpKind dump_type = RawOnly);
  void dump(const auto *data, size_t len) const;

private:
  void accumulate(Accumulate type, size_t bytes);
  bool isReady() const { return socket.is_open(); };
  bool isReady(const error_code &ec, const src_loc loc = src_loc::current());

  // receives the rx_bytes from async_read
  void handleAudioBuffer(size_t bytes);
  void nextAudioBuffer();
  bool rxAtLeast(size_t bytes = 1);
  bool rxAvailable(); // load bytes immediately available
  packet::In &wire() { return _wire; }
  void wireToPacket();

  static ccs fnName(src_loc loc = src_loc::current()) { return loc.function_name(); }

private:
  // order dependent - initialized by constructor
  tcp_socket socket;

  packet::In _wire;

  uint64_t _rx_bytes = 0;
  uint64_t _tx_bytes = 0;

  bool _shutdown = false;

  // private:
  //   static constexpr auto re_syntax = std::regex_constants::ECMAScript;
};

} // namespace buffered
} // namespace audio
} // namespace rtp
} // namespace pierre