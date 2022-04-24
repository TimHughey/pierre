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

#include <cstdint>
#include <exception>
#include <fmt/format.h>
#include <source_location>

#include "rtsp/server.hpp"
#include "rtsp/session.hpp"
#include <core/host.hpp>
#include <core/service.hpp>
#include <mdns/mdns.hpp>
#include <nptp/nptp.hpp>

namespace pierre {
namespace rtsp {

using namespace boost::asio;
using namespace boost::system;
using error_code = boost::system::error_code;
using src_loc = std::source_location;

Server::Server(const Server::Opts &opts)
    : opts(opts), io_ctx(opts.io_ctx), acceptor{io_ctx, tcp_endpoint(ip_tcp::v6(), _port)} {
  // store a reference to the io_ctx and create the acceptor
  // see start()
}

void Server::async_accept() {
  acceptor.async_accept(_sockets.emplace_back(io_ctx), [&](error_code ec) {
    const src_loc loc = std::source_location::current();

    tcp_socket &socket = _sockets.back(); // grab the socket just accepted

    if (ec == errc::success) {
      auto handle = socket.native_handle();
      fmt::print("{} accepted connection, handle={}\n", loc.function_name(), handle);

      // auto session = Session::create(
      //     std::move(*socket), {.host = opts.host, .service = opts.service, .mdns = opts.mdns});

      auto session = Session::create(
          socket,
          {.host = opts.host, .service = opts.service, .mdns = opts.mdns, .nptp = opts.nptp});

      _sessions.emplace_back(session);

      session->start();
    } else {
      fmt::print("{} accept connection failed, error={}\n", ec.message());
    }

    async_accept();
  });
}

void Server::start() {
  try {
    async_accept();
  } catch (std::exception &e) {
    const src_loc loc = std::source_location::current();
    fmt::print("{} caught {}\n", loc.function_name(), e.what());
  }
}

} // namespace rtsp
} // namespace pierre