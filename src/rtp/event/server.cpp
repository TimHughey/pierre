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

#include <fmt/format.h>
#include <source_location>

#include "event/server.hpp"
#include "event/session.hpp"
#include "rtp/port_promise.hpp"

namespace pierre {
namespace rtp {
namespace event {

using namespace boost::system;
using io_context = boost::asio::io_context;
using tcp_acceptor = boost::asio::ip::tcp::acceptor;
using tcp_endpoint = boost::asio::ip::tcp::endpoint;
using tcp_socket = boost::asio::ip::tcp::socket;
using ip_tcp = boost::asio::ip::tcp;
using error_code = boost::system::error_code;
using src_loc = std::source_location;

Server::Server(io_context &io_ctx)
    : io_ctx(io_ctx), acceptor(io_ctx, tcp_endpoint(ip_tcp::v6(), _port)) {
  _port = acceptor.local_endpoint().port();

  _port_promise.set_value(_port); // used to inform the creator of the choosen port
}

Server::~Server() {
  // more later
}

void Server::asyncAccept() {
  // capture the io_ctx in this optional for use when a request is accepted
  // call it socket since it will become one once accepted
  socket.emplace(io_ctx);

  // since the io_ctx is wrapped in the optional and async_accept wants the actual
  // io_ctx we must deference or get the value of the optional
  acceptor.async_accept(*socket, [&](error_code ec) {
    if (ec == errc::success) {
      auto handle = (*socket).native_handle();
      fmt::print("{} accepted connection, handle={}\n", fnName(), handle);

      // create the session passing all the options
      // notes
      //  1: we move the socket (value of the optional) to session
      //  2. we then start the session using the shared_ptr returned by Session::create()
      //  3. Session::asyncRequestLoop() must ensure it captures the shared_ptr
      //     to ensure the Session stays in scope
      auto session = Session::create(std::move(socket.value()));

      session->asyncEventLoop();

    } else {
      fmt::print("{} accept connection failed, error={}\n", ec.message());
    }

    asyncAccept();
  });
}

PortFuture Server::start() {
  // attach async_accept as work to the io_ctx
  asyncAccept();

  // return the selected port to the caller
  return _port_promise.get_future();
}

} // namespace event
} // namespace rtp
} // namespace pierre
