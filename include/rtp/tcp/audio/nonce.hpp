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

#pragma once

#include <boost/asio/buffer.hpp>
#include <cstdint>
#include <source_location>
#include <string.h>
#include <string_view>
#include <vector>

namespace pierre {
namespace rtp {
namespace tcp {
namespace audio {

struct Nonce {
  char bytes[12] = {0};

public:
  Nonce(uint8_t *packet, size_t size) {
    // offset to the packet nonce is:
    // size of the packet - hdr - nonce + 4
    // the +4 is because the packet nonce is only 8 bytes and the expected
    // nonce is 12 bytes
    const size_t offset = size - rfc3550_hdr::size() - sizeof(Nonce) + 4;

    char *pkt_nonce = (char *)packet + offset;
    char *nonce = bytes + 4;

    // copy the 8 bytes of packet nonce into the lsb of the actual nonce
    memcpy(nonce, pkt_nonce, sizeof(Nonce) - 4);
  }

  const char *data() { return bytes; }
};

} // namespace audio
} // namespace tcp
} // namespace rtp
} // namespace pierre