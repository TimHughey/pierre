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

namespace pierre {
namespace pcm {

struct rfc3550_hdr {
  /*
  credit to https://emanuelecozzi.net/docs/airplay2/rt for the packet info

  RFC 3550
      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 0x0  |V=2|P|X|  CC   |M|     PT      |       sequence number         |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 0x4  |                           timestamp                           |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 0x8  |           synchronization source (SSRC) identifier            |
      +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 0xc  |            contributing source (CSRC) identifiers             |
      |                             ....                              |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


  Apple Reuse of RFC3550 header
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

  // the purpose of this struct is to provide structure to raw uint8_t
  // data. it is essential the member variables below remain in this
  // specific order and additional class members are not added

  uint8_t vpxcc;   // version, padding, extension, CSRC count
  uint8_t mpt;     // marker bit, payload type
  uint16_t seqnum; // sequence num
  union {          // union of pure rfc3350 or Apple's defintion
    struct {
      uint16_t timestmp; // timestamp
      uint16_t ssrc;     // sync source id
    } rfc3550;           // as defined by the standard

    union {
      uint32_t timestamp; // timestamp
      uint32_t bytes[2];  // aad as an array
      uint64_t full;      // a single uint64 of aad
    } aad;                // as defined by Apple
  };

public:
  bool isValid() const { return ((type() == STANDARD) || type() == RESEND); }
  static rfc3550_hdr *from(uint8_t *data) { return (rfc3550_hdr *)data; };
  uint32_t timestamp() const { return aad.timestamp; }
  uint16_t seqNum() const { return (mpt << 16) + seqnum; }
  static constexpr size_t size() { return sizeof(rfc3550_hdr); }
  uint8_t type() const { return mpt & ~0x80; }
  static constexpr size_t validBytes() { return sizeof(vpxcc) + sizeof(mpt); }
  uint8_t version() const { return (vpxcc & 0xc0 >> 6); }

  static constexpr uint8_t STANDARD = 0x60;
  static constexpr uint8_t RESEND = 0x56;
};

} // namespace pcm
} // namespace pierre
