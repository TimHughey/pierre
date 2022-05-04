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
#include <iterator>
#include <list>
#include <memory>
#include <source_location>

#include "packet/rfc3550/hdr.hpp"
#include "packet/rfc3550/trl.hpp"

namespace pierre {
namespace packet {
namespace rfc3550 {

void trl::build(const uint8_t *src, size_t len) {
  clear();

  // minimum size is hdr + trl (leaving no room for actual payload)
  if (len < (hdr::size() + size())) {
    return;
  }

  // apparently the last eight (8) bytes are the nonce,
  auto *nptr = (unsigned char *)src + len - sizeof(NonceMini);

  // the frame nonce is eight (8) bytes while a full nonce is 12 bytes.
  // leave the first four bytes emply

  for (size_t idx = 4; idx < sizeof(Nonce); idx++) {
    nonce[idx] = *nptr++; // copy the byte
  }

  // // tlr_ptr is pointing to the beginning of tags
  // for (size_t idx = 0; idx < sizeof(Tags); idx++) {
  //   tags[idx] = src[src_idx++];
  // }

  // note: don't use tlr_ptr any further it now points beyond the
  // end of the packet
}

void trl::clear() {
  nonce.fill(0x00);
  tags.fill(0x00);
}

void trl::dump(const src_loc loc) const {
  fmt::print(FMT_STRING("{} sizeof={}\n{}\n"), fnName(loc), size(), dumpString());
}

const trl::string trl::dumpString() const {
  auto constexpr fi = FMT_STRING("{:02}:{:#04x}"); // i.e. 00[0xdf] 01[0xa0] ...

  std::list<std::string> byte_str_list; // array of strings for each byte

  // create string of the nonce bytes
  for (size_t idx = 0; idx < nonce.size(); idx++) {
    byte_str_list.emplace_back(fmt::format(fi, idx, nonce[idx]));
  }

  auto nonce_string = fmt::format("{}", fmt::join(byte_str_list, " "));

  byte_str_list.clear(); // reuse the same list

  for (size_t idx = 0; idx < tags.size(); idx++) {
    byte_str_list.emplace_back(fmt::format(fi, idx, tags[idx]));
  }

  auto tags_string = fmt::format("{}", fmt::join(byte_str_list, " "));

  // put the two together
  return fmt::format("\t nonce: {}\n\t tags : {}", nonce_string, tags_string);
}

} // namespace rfc3550
} // namespace packet
} // namespace pierre
