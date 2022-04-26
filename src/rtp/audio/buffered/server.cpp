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

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdint>
#include <exception>
#include <fmt/format.h>
#include <source_location>

#include "rtp/audio/buffered/server.hpp"
#include "rtp/audio/buffered/session.hpp"

namespace pierre {
namespace rtp {
namespace audio {
namespace buffered {

using namespace boost::system;
using io_context = boost::asio::io_context;
using tcp_acceptor = boost::asio::ip::tcp::acceptor;
using tcp_endpoint = boost::asio::ip::tcp::endpoint;
using tcp_socket = boost::asio::ip::tcp::socket;
using ip_tcp = boost::asio::ip::tcp;
using error_code = boost::system::error_code;
using src_loc = std::source_location;

Server::Server(io_context &io_ctx)
    : io_ctx(io_ctx), acceptor{io_ctx, tcp_endpoint(ip_tcp::v6(), port)} {
  // store a reference to the io_ctx and create the acceptor

  // with the endpoint created we know the port selected, grab it
  // and put in the promise to the original caller.  the caller needs to
  // know the port for an AirPlay2 request reply
  port = acceptor.local_endpoint().port();
}

void Server::asyncAccept() {
  // capture the io_ctx in this optional for use when a request is accepted
  // call it socket since it will become one once accepted
  socket.emplace(io_ctx);

  // since the io_ctx is wrapped in the optional and asyncAccept wants the actual
  // io_ctx we must deference or get the value of the optional
  acceptor.async_accept(*socket, [&](error_code ec) {
    if (ec == errc::success) {
      // const std::source_location loc = std::source_location::current();
      // auto handle = (*socket).native_handle();
      // fmt::print("{} accepted connection, handle={}\n", loc.function_name(), handle);

      // create the session passing all the options
      // notes
      //  1: we move the socket (value of the optional) to session
      //  2. we then start the session using the shared_ptr returned by Session::create()
      //  3. Session::start() must ensure the shared_ptr pointer is captured in the
      //     async lamba so it doesn't go out of scope
      auto session = Session::create(std::move(socket.value()));

      session->asyncAudioBufferLoop();

    } else {
      fmt::print("{} accept connection failed, error={}\n", ec.message());
    }

    asyncAccept();
  });
}

uint16_t Server::localPort() {
  // only start the server once
  if (live == false) {
    asyncAccept();
    live = true;
  }

  return port;
}

} // namespace buffered
} // namespace audio
} // namespace rtp
} // namespace pierre