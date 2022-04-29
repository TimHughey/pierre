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

#include "rtp/control/datagram.hpp"

namespace pierre {
namespace rtp {
namespace control {

using namespace boost;
using namespace boost::system;

using io_context = boost::asio::io_context;
using udp_endpoint = boost::asio::ip::udp::endpoint;
using udp_socket = boost::asio::ip::udp::socket;
using ip_udp = boost::asio::ip::udp;

Datagram::Datagram(io_context &io_ctx)
    : io_ctx(io_ctx), socket(io_ctx, udp_endpoint(ip_udp::v6(), port)) {
  // local endpoint is created, capture the port for the original caller
  port = socket.local_endpoint().port();
  _wire.resize(4096);
}

void Datagram::asyncControlLoop() {
  // notes:
  //  1. nothing within this function can be captured by the lamba
  //     because the scope of this function ends before
  //     the lamba executes however we can reference member variables
  //     in the call to socket.async_receive_from()
  //
  //  2. socket.async_receive_from() will attach the lamba to the io_ctx
  //     then immediately return and then this function returns
  //
  //  3. we capture a shared_ptr (self) for access within the lamba,
  //     that shared_ptr is kept in scope while async_receive_from is
  //     waiting for data on the socket then during execution
  //     of the lamba
  //
  //  4. when called again from within the lamba the sequence of
  //     events repeats (this function returns) and the shared_ptr
  //     once again goes out of scope
  //
  //  5. the crucial point -- we must keep the use count
  //     of the session above zero until the session ends
  //     (e.g. due to error, natural completion, io_ctx is stopped)
  //
  //  6. key difference between TCP and UDP server is there are no
  //     asio free functions for UDP

  socket.async_receive_from(
      asio::buffer(wire()), endpoint, [self = shared_from_this()](error_code ec, size_t rx_bytes) {
        // general notes:
        //
        // 1. this was not captured so the lamba is not in 'this' context
        // 2. all calls to the session must be via self (which was captured)
        // 3. we do minimal activities to quickly get to 'this' context

        // essential activies:
        //
        // 1. check the error code
        if (self->isReady(ec)) {
          // 2. handle the request (remember the data received is in the wire buffer)
          //    handleRequest will perform the request/reply transaction (serially)
          //    and returns when the transaction completes or errors
          self->handleControlBlock(rx_bytes);

          if (self->isReady()) {
            // 3. call nextRequest() to reset buffers and state
            self->nextControlBlock();

            // 4. the socket is still ready, call asyncRequest to await next request
            // 5. async_read captures a fresh shared_ptr
            // 6. reminder... call returns when async_read registers the work with io_ctx
            self->asyncControlLoop();
            // 7. falls through
          }
        } // self is about to go out of scope...
      }); // self is out of scope and the shared_ptr use count is reduced by one

  // final notes:
  // 1. the first return of this function traverses back to the Server that created the
  //    Session (in the same io_ctx)
  // 2. subsequent returns are to the io_ctx and match the required void return signature
}

void Datagram::handleControlBlock(size_t bytes) {
  fmt::print("{} received control block bytes={}\n", fnName(), bytes);
}

bool Datagram::isReady(const error_code &ec, const src_loc loc) {
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

uint16_t Datagram::localPort() {
  if (live == false) {
    asyncControlLoop();
    live = true;
  }

  return port;
}

void Datagram::nextControlBlock() {
  fmt::print("{} wire_bytes={}\n", fnName(), _wire.size());

  // reset all buffers and state
  _wire.clear();
}

void Datagram::teardown() {
  [[maybe_unused]] error_code ec;

  socket.cancel(ec);
  socket.shutdown(udp_socket::shutdown_both, ec);
  socket.close(ec);
}

} // namespace control
} // namespace rtp
} // namespace pierre
