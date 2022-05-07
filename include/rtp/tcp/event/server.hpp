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
#include <boost/asio/ip/tcp.hpp>
#include <future>
#include <memory>
#include <optional>
#include <string>

namespace pierre {
namespace rtp {
namespace tcp {

class EventServer {
public:
  using io_context = boost::asio::io_context;
  using tcp_acceptor = boost::asio::ip::tcp::acceptor;
  using tcp_endpoint = boost::asio::ip::tcp::endpoint;
  using tcp_socket = boost::asio::ip::tcp::socket;
  using ip_tcp = boost::asio::ip::tcp;
  using error_code = boost::system::error_code;
  using src_loc = std::source_location;

  typedef const char *ccs;

public:
  EventServer(io_context &io_ctx)
      : io_ctx(io_ctx), acceptor(io_ctx, tcp_endpoint(ip_tcp::v6(), ANY_PORT)) {
    // 1. store a reference to the io_ctx
    // 2. create the acceptor
  }

  // ensure server is started and return the local endpoint port
  uint16_t localPort() {
    if (live == false) {
      asyncAccept();
      live = true;
    }

    return acceptor.local_endpoint().port();
  }

  void teardown() {
    [[maybe_unused]] error_code ec;
    acceptor.close(ec);
  }

private:
  // start accepting connections
  void asyncAccept();

  // check error code and either add more work or gracefully exit
  void asyncAccept(const error_code &ec);

private:
  static ccs fnName(src_loc loc = src_loc::current()) { return loc.function_name(); }

private:
  // order dependent
  io_context &io_ctx;
  tcp_acceptor acceptor;

  bool live = false;

  // temporary holder of socket (io_ctx) while waiting for a connection
  std::optional<tcp_socket> socket;

  static constexpr uint16_t ANY_PORT = 0;
};

} // namespace tcp
} // namespace rtp
} // namespace pierre
