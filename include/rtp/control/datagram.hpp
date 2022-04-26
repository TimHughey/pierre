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
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <future>
#include <memory>
#include <source_location>
#include <string>
#include <thread>

#include "packet/in.hpp"

namespace pierre {
namespace rtp {
namespace control {

class Datagram; // forward decl for typedef
typedef std::shared_ptr<Datagram> sDatagram;

class Datagram : public std::enable_shared_from_this<Datagram> {
public:
  using error_code = boost::system::error_code;
  using io_context = boost::asio::io_context;
  using udp_endpoint = boost::asio::ip::udp::endpoint;
  using udp_socket = boost::asio::ip::udp::socket;
  using ip_udp = boost::asio::ip::udp;
  using src_loc = std::source_location;
  typedef const char *ccs;

public:
  ~Datagram();

public: // object creation and shared_ptr API
  [[nodiscard]] static sDatagram create(io_context &io_ctx) {
    // not using std::make_shared; constructor is private
    return sDatagram(new Datagram(io_ctx));
  }

  sDatagram getSelf() { return shared_from_this(); }

private:
  Datagram(io_context &io_ctx);

public:
  // Public API
  void asyncControlLoop();
  uint16_t localPort();

private:
  bool isReady() const { return socket.is_open(); };
  bool isReady(const error_code &ec, const src_loc loc = src_loc::current());

  void handleControlBlock(size_t bytes);
  void nextControlBlock();

  packet::In &wire() { return _wire; }

  static ccs fnName(src_loc loc = src_loc::current()) { return loc.function_name(); }

private:
  // order dependent
  io_context &io_ctx;
  udp_socket socket;

  uint16_t port = 0; // choose any port
  bool live = false;

  // latest sender endpoint
  udp_endpoint endpoint;

  packet::In _wire;
  uint64_t _rx_bytes = 0;
  uint64_t _tx_bytes = 0;
};

} // namespace control
} // namespace rtp
} // namespace pierre
