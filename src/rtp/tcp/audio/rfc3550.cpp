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

#include <algorithm>
#include <cstdint>
#include <fmt/format.h>
#include <memory>
#include <source_location>

#include "rtp/tcp/audio/rfc3550.hpp"
#include "rtp/tcp/audio/rfc3550/header.hpp"
#include "rtp/tcp/audio/rfc3550/trailer.hpp"

namespace pierre {
namespace rtp {
namespace tcp {
namespace audio {

rfc3550::rfc3550(RawData &raw, size_t rx_bytes) : _raw(raw), _rx_bytes(rx_bytes) {
  // more later
}

// locate the marker in the raw packet
// this method should only be called after initial creation
// and before reduce
bool rfc3550::reduceToFirstMarker() {
  const size_t packet_min = rfc3550_hdr::size() + rfc3550_trl::size();

  // must have received enough bytes to make sense
  if (_rx_bytes < packet_min) {
    return false;
  }

  size_t marker_pos = 0;
  bool marker_found = false;

  auto ptr = _raw.data();

  // stop looking when there's not enough bytes left for the rfc3550 packet
  for (size_t idx = 0; idx < (_rx_bytes - packet_min); idx++) {
    const auto hdr = rfc3550_hdr::from(ptr++);
    type = hdr->mpt & ~0x80;
    v2 = hdr->vpxcc & 0x40;       // RTP v2
    marker_bit = hdr->mpt & 0x80; // marker bit set

    if (v2 && marker_bit) {
      if ((type == STANDARD) || (type == RESEND)) {
        packet_timestamp = hdr->aad.timestamp;
        seq_num = hdr->seqnum;

        marker_pos = idx;
        marker_found = true;
        break;
      }
    }
  }

  if (marker_found == false) {
    fmt::print(FMT_STRING("{} no marker rx_bytes={}\n"), fnName(), _rx_bytes);
    return false;
  }

  _tossed_bytes = _rx_bytes - marker_pos;

  auto begin = _raw.begin();
  std::advance(begin, marker_pos);

  auto end = _raw.end();
  std::advance(end, (_rx_bytes - marker_pos) * -1);
  auto reduced = std::vector<uint8_t>(begin, end);

  std::swap(_raw, reduced);

  dumpHeader();

  return marker_found;
}

void rfc3550::dumpHeader(const src_loc loc) {
  auto fmt_str = FMT_STRING("{}  tossed={:>05}  pt={:#04x}  seqnum={:>8}  tsmp={:>14}\n");

  fmt::print(fmt_str, fnName(loc), _tossed_bytes, type, seq_num, packet_timestamp);
}

} // namespace audio
} // namespace tcp
} // namespace rtp
} // namespace pierre