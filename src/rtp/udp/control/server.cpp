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

#include <array>
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/read.hpp>
#include <fmt/format.h>
#include <future>
#include <source_location>

#include "rtp/udp/control/server.hpp"

namespace pierre {
namespace rtp {
namespace udp {

using namespace boost;
using namespace boost::system;

using io_context = boost::asio::io_context;
using udp_endpoint = boost::asio::ip::udp::endpoint;
using udp_socket = boost::asio::ip::udp::socket;
using ip_udp = boost::asio::ip::udp;

void ControlServer::asyncControlLoop() {
  // notes:
  //  1. for this datagram server we don't use a shared_ptr so we
  //     we can capture this
  //
  //  2. socket.async_receive_from() associates lamba to the io_ctx
  //     then immediately returns and this function returns
  //
  //  3. key difference between TCP and UDP server is there are no
  //     asio free functions for UDP

  socket.async_receive_from(asio::buffer(wire()), endpoint, [&](error_code ec, size_t rx_bytes) {
    // essential activies:
    //
    // 1. check the error code
    if (isReady(ec)) {
      // 2. handle the request (remember the data received is in the wire buffer)
      //    handleRequest will perform the request/reply transaction (serially)
      //    and returns when the transaction completes or errors
      handleControlBlock(rx_bytes);

      if (isReady()) {
        // 3. call nextRequest() to reset buffers and state
        nextControlBlock();

        // 4. the socket is still ready, call asyncRequest to await next request
        // 5. reminder... call returns when async_read registers the work with io_ctx
        asyncControlLoop();
        // 7. falls through
      }
    }
  });

  // final notes:
  // 1. the first return of this function traverses back to the Server that created the
  //    Session (in the same io_ctx)
  // 2. subsequent returns are to the io_ctx and match the required void return signature
}

void ControlServer::handleControlBlock(size_t bytes) {
  fmt::print("{} received control block bytes={}\n", fnName(), bytes);
}

bool ControlServer::isReady(const error_code &ec, const src_loc loc) {
  [[maybe_unused]] error_code __ec;
  auto rc = isReady();

  if (rc) {
    switch (ec.value()) {
      case errc::success:
        break;

      default:
        fmt::print("{} SHUTDOWN socket={} err_value={} msg={}\n", loc.function_name(),
                   socket.native_handle(), ec.value(), ec.message());

        socket.shutdown(udp_socket::shutdown_both, __ec);
        rc = false;
    }
  }

  return rc;
}

void ControlServer::nextControlBlock() {
  fmt::print("{} wire_bytes={}\n", fnName(), _wire.size());

  // reset all buffers and state
  _wire.reset();
}

} // namespace udp
} // namespace rtp
} // namespace pierre
