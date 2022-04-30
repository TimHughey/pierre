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

#include "rtp/udp/control/packet.hpp"

namespace pierre {
namespace rtp {
namespace udp {

class ControlServer {
public:
  using io_context = boost::asio::io_context;
  using udp_endpoint = boost::asio::ip::udp::endpoint;
  using udp_socket = boost::asio::ip::udp::socket;
  using ip_udp = boost::asio::ip::udp;
  using error_code = boost::system::error_code;
  using src_loc = std::source_location;
  typedef const char *ccs;

public:
  // create the ControlServer
  ControlServer(io_context &io_ctx)
      : io_ctx(io_ctx), socket(io_ctx, udp_endpoint(ip_udp::v4(), ANY_PORT)) {
    _wire.resize(4096);
  }

  // ensure the server is started and return the local endpoint port
  uint16_t localPort() {
    if (live == false) {
      asyncControlLoop();
      live = true;
    }

    return socket.local_endpoint().port();
  }

  void teardown() {
    [[maybe_unused]] error_code ec;

    socket.cancel(ec);
    socket.shutdown(udp_socket::shutdown_both, ec);
    socket.close(ec);
  }

private:
  void asyncControlLoop();
  bool isReady() const { return socket.is_open(); };
  bool isReady(const error_code &ec, const src_loc loc = src_loc::current());

  void handleControlBlock(size_t bytes);
  void nextControlBlock();

  control::Packet &wire() { return _wire; }

  static ccs fnName(src_loc loc = src_loc::current()) { return loc.function_name(); }

private:
  // order dependent
  io_context &io_ctx;
  udp_socket socket;

  bool live = false;

  // latest sender endpoint
  udp_endpoint endpoint;

  control::Packet _wire;
  uint64_t _rx_bytes = 0;
  uint64_t _tx_bytes = 0;

  static constexpr uint16_t ANY_PORT = 0;
};

} // namespace udp
} // namespace rtp
} // namespace pierre
