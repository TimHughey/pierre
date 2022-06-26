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

#include "rtp/udp/control/packet.hpp"

#include <arpa/inet.h>
#include <fmt/format.h>

namespace pierre {
namespace rtp {
namespace udp {
namespace control {

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
} // namespace udp
} // namespace rtp
} // namespace pierre