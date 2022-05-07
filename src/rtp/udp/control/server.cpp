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

void ControlServer::asyncHeader() {
  // notes:
  //  1. for this datagram server we don't use a shared_ptr so we
  //     we can capture this
  //
  //  2. socket.async_receive_from() associates lamba to the io_ctx
  //     then immediately returns and this function returns
  //
  //  3. key difference between TCP and UDP server is there are no
  //     asio free functions for UDP

  if (true) { // debug
    constexpr auto f = FMT_STRING("{} socket is_open={} port={}\n");
    fmt::print(f, fnName(), socket.is_open(), socket.local_endpoint().port());
  }

  auto buff_hdr = asio::buffer(hdrData(), hdrSize());

  socket.async_receive_from(buff_hdr, remote_endpoint, [&](error_code ec, size_t rx_bytes) {
    if (isReady(ec)) {
      _hdr.loaded(rx_bytes);
      _hdr.dump();

      if (_hdr.moreBytes()) {
        asyncRestOfPacket();
      } else {
        nextBlock();
        asyncHeader();
      }
    }
  });
}

void ControlServer::asyncRestOfPacket() {
  // the length specified in the header denotes the entire packet size

  auto buff_rest = asio::buffer(_wire.data(), _hdr.moreBytes());
  socket.async_receive_from(buff_rest, remote_endpoint, [&](error_code ec, size_t rx_bytes) {
    if (isReady(ec)) {
      if (isReady(ec)) { // debug
        if (true) {
          constexpr auto f = FMT_STRING("{} rx_bytes={}\n");
          fmt::print(f, fnName(), rx_bytes);
        }

        nextBlock();
        asyncHeader(); // schedule more work
      }
    }
  });
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

void ControlServer::nextBlock() {
  fmt::print("{} wire_bytes={}\n", fnName(), _wire.size());

  // reset all buffers and state
  _hdr.clear();
  _wire.clear();
}

} // namespace udp
} // namespace rtp
} // namespace pierre
