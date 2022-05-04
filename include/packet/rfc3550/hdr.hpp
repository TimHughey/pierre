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

#include <arpa/inet.h>
#include <cstdint>
#include <source_location>
#include <string>
#include <vector>

namespace pierre {
namespace packet {
namespace rfc3550 {

struct hdr {
  /*
  credit to https://emanuelecozzi.net/docs/airplay2/rt for the packet info

  RFC3550 header (as tweaked by Apple)
       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       ---------------------------------------------------------------
 0x0  | V |P|X|  CC   |M|     PT      |       Sequence Number         |
      |---------------------------------------------------------------|
 0x4  |                        Timestamp (AAD[0])                     |
      |---------------------------------------------------------------|
 0x8  |                          SSRC (AAD[1])                        |
      |---------------------------------------------------------------|
 0xc  :                                                               :

  */

  // RFC3550 header
  uint8_t vpxcc;   // version, padding, extension, CSRC count
  uint8_t mpt;     // marker bit, payload type
  uint32_t seqnum; // sequence nu

  union {
    uint32_t timestamp; // timestamp
    uint32_t bytes[2];  // aad as an array
    uint64_t full;      // a single uint64 of aad
  } aad;                // as defined by Apple

private:
  using src_loc = std::source_location;
  using string = std::string;
  typedef const char *ccs;

public:
  hdr() { clear(); } // empty, allow for placeholders
  hdr(const std::vector<uint8_t> &src) { build(src.data(), src.size()); }
  hdr(const uint8_t *src, size_t len) { build(src, len); }

  // struct operations
  void clear();
  static constexpr size_t size() { return 12; }

  // validation determined by observation
  bool isValid() const { return version() == 0x02; }

  // getters
  uint16_t seqNum() const { return ntohs(seqnum); }
  uint32_t seqNum32() const { return (mpt * 0x10000) + seqNum(); }
  uint32_t timestamp() const { return ntohl(aad.timestamp); }
  uint8_t type() const { return mpt & ~0x80; }
  uint8_t version() const { return ((vpxcc & 0xc0) >> 6); }

  // misc debug
  void dump(const src_loc loc = src_loc::current()) const;
  const string dumpString() const;

private:
  void build(const uint8_t *src, size_t len);
  ccs fnName(const src_loc loc = src_loc::current()) const { return loc.function_name(); }
};

} // namespace rfc3550
} // namespace packet
} // namespace pierre
