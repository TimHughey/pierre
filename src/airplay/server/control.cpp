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

void Control::asyncLoop() {
  // notes:
  //  1. for this datagram server we don't use a shared_ptr so we
  //     we can capture this
  //
  //  2. socket.async_receive_from() associates lamba to the io_ctx
  //     then immediately returns and this function returns
  //
  //  3. key difference between TCP and UDP server is there are no
  //     asio free functions for UDP

  if (false) { // debug
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
        asyncLoop();
      }
    }
  });
}

void Control::asyncRestOfPacket() {
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
        asyncLoop(); // schedule more work
      }
    }
  });
}

bool Control::isReady(const error_code &ec, const src_loc loc) {
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

void Control::nextBlock() {
  fmt::print("{} wire_bytes={}\n", fnName(), _wire.size());

  // reset all buffers and state
  _hdr.clear();
  _wire.clear();
}

} // namespace server
} // namespace airplay
} // namespace pierre
