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

#include "server/control.hpp"

#include <fmt/format.h>

namespace pierre {
namespace airplay {
namespace server {

namespace control { // control packet

void hdr::clear() {
  full.vpm = 0x00;
  full.type = 0x00;
  full.length = 0x00000; // 16 bits
}

void hdr::loaded(size_t rx_bytes) {
  if (rx_bytes == sizeof(full)) {
    // clean up possible strict aliasing issues
    full.vpm = data()[0];
    full.type = data()[1];

    full.length = ntohs(full.length);

    if (true) { // debug
      constexpr auto f = FMT_STRING("{} length={}\n");
      fmt::print(f, fnName(), length());
    } else {
      constexpr auto f = FMT_STRING("{} bad length={}\n");
      fmt::print(f, fnName(), length());
    }
  }
}

void hdr::dump(csrc_loc loc) const {
  constexpr auto f = FMT_STRING("{} vsn={:#04x} padding={:5} marker={:5}");
  fmt::print(f, fnName(loc), version(), marker(), padding());
}

void Packet::loaded([[maybe_unused]] size_t rx_bytes) {
  _size = rx_bytes;
  _valid = true;
}

} // namespace control

// back to namespace server

using namespace boost;
using namespace boost::system;
using namespace asio;

Control::Control(const Inject &di)
    : Base("CONTROL SERVER"),                              // server name
      io_ctx(di.io_ctx),                                   // io_ctx
      socket(io_ctx, udp_endpoint(ip_udp::v4(), ANY_PORT)) // create socket and endpoint
{
  _wire.clear();
}

Control::~Control() {
  if (false) { // debug
    constexpr auto f = FMT_STRING("{} {} shutdown handle={}\n");
    fmt::print(f, runTicks(), serverId(), socket.native_handle());
  }

  [[maybe_unused]] error_code ec; // must use error_code overload to prevent throws

  socket.shutdown(udp_socket::shutdown_both, ec);
  socket.close(ec);
}

void Control::asyncLoop(const error_code ec_last) {
  // notes:
  //  1. for this datagram server we don't use a shared_ptr so we
  //     we can capture this
  //
  //  2. socket.async_receive_from() associates lamba to the io_ctx
  //     then immediately returns and this function returns
  //
  //  3. key difference between TCP and UDP server is there are no
  //     asio free functions for UDP

  // first things first, check ec_last passed in, bail out if needed
  if ((ec_last != errc::success) || !socket.is_open()) { // problem

    // don't highlight "normal" shutdown
    // if ((ec_last.value() != errc::operation_canceled) &&
    //     (ec_last.value() != errc::resource_unavailable_try_again)) {
    constexpr auto f = FMT_STRING("{} {} asyncLoop failed, error={}\n");
    fmt::print(f, runTicks(), serverId(), ec_last.message());
    // }
    // some kind of error occurred, simply close the socket
    [[maybe_unused]] error_code __ec;
    socket.close(__ec); // used error code overload to prevent throws

    return; // bail out
  }

  auto buff_hdr = asio::buffer(hdrData(), hdrSize());

  if (false) { // debug
    constexpr auto f = FMT_STRING("{} {} socket open handle={}\n");
    fmt::print(f, runTicks(), serverId(), socket.native_handle());
  }

  socket.async_receive_from(buff_hdr, remote_endpoint, [&](error_code ec, size_t rx_bytes) {
    if (isReady(ec)) {
      _hdr.loaded(rx_bytes);
      _hdr.dump();

      if (_hdr.moreBytes()) {
        asyncRestOfPacket();
      } else {
        nextBlock();
        asyncLoop(ec);
      }
    }
  });
}

void Control::asyncRestOfPacket() {
  // the length specified in the header denotes the entire packet size

  auto buff_rest = asio::buffer(_wire.data(), _hdr.moreBytes());
  socket.async_receive_from(buff_rest, remote_endpoint, [&](error_code ec, size_t rx_bytes) {
    if (isReady(ec)) { // debug

      if (true) {
        constexpr auto f = FMT_STRING("{} rx_bytes={}\n");
        fmt::print(f, fnName(), rx_bytes);
      }

      nextBlock();
      asyncLoop(ec); // schedule more work
    }
  });
}

bool Control::isReady(const error_code &ec, [[maybe_unused]] csrc_loc loc) {
  [[maybe_unused]] error_code __ec;
  auto rc = isReady();

  if (rc) {
    switch (ec.value()) {
      case errc::success:
        break;

      default:
        if (false) { // debug
          constexpr auto f = FMT_STRING("{} {} SHUTDOWN socket={} msg={}\n");
          fmt::print(f, runTicks(), serverId(), socket.native_handle(), ec.message());
        }

        socket.shutdown(udp_socket::shutdown_both, __ec);
        rc = false;
    }
  }

  return rc;
}

void Control::nextBlock() {
  if (true) { // debug
    constexpr auto f = FMT_STRING("{} {} wire_bytes={}\n");
    fmt::print(f, runTicks(), serverId(), _wire.size());
  }

  // reset all buffers and state
  _hdr.clear();
  _wire.clear();
}

void Control::teardown() {
  // here we only issue the cancel to the acceptor.
  // the closing of the acceptor will be handled when
  // the error is caught by asyncLoop

  [[maybe_unused]] auto handle = socket.native_handle();
  [[maybe_unused]] error_code ec;
  socket.cancel(ec);

  __LOGX("{} teardown socket={} error={}\n", serverId(), handle, ec.message());
}

} // namespace server
} // namespace airplay
} // namespace pierre
