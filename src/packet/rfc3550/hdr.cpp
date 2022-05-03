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

#include "packet/rfc3550/hdr.hpp"

namespace pierre {
namespace packet {
namespace rfc3550 {

void hdr::build(const uint8_t *src, size_t len) {
  clear();

  if (len < size()) {
    return;
  }

  // copy the src data into the struct to prevent running
  // afoul of string aliasing.  by setting each element to
  // either the specific byte or a calculated value we are
  // assured to comply with strict alias requirements
  vpxcc = *src++;
  mpt = *src++;

  seqnum += *src++ << 16;
  seqnum += *src++;

  for (int byte = sizeof(aad.full) - 1; byte >= 0; byte--) {
    aad.full += *src++ << (byte * 8);
  }
}

void hdr::clear() {
  vpxcc = 0x00;
  mpt = 0x00;
  seqnum = 0x00;
  aad.full = 0x00;
}

void hdr::dump(const src_loc loc) const {
  fmt::print(FMT_STRING("{}  {}\n"), fnName(loc), dumpString());
}

const hdr::string hdr::dumpString() const {
  auto constexpr f = FMT_STRING("valid={:<5}  seqnum={:>05}  seqnum32= {:>06}  tsmp={:>012}");

  return fmt::format(f, isValid(), seqNum(), seqNum32(), timestamp());
}

} // namespace rfc3550
} // namespace packet
} // namespace pierre
