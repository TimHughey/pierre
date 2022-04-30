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

#include <arpa/inet.h>
#include <fmt/format.h>

#include "rtp/udp/control/packet.hpp"

namespace pierre {
namespace rtp {
namespace udp {
namespace control {

void Packet::loaded([[maybe_unused]] size_t rx_bytes) {
  // fmt::print(FMT_STRING("{} rx_bytes={:>6}\n"), fnName(), rx_bytes);

  _valid = true;
}

void Packet::reset() {
  clear();
  resize(STD_PACKET_SIZE);
}

} // namespace control
} // namespace udp
} // namespace rtp
} // namespace pierre