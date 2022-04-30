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

#include <cstdint>
#include <source_location>
#include <string_view>
#include <vector>

namespace pierre {
namespace rtp {
namespace tcp {
namespace audio {

class Packet : public std::vector<uint8_t> {
public:
  using src_loc = std::source_location;
  typedef const std::string_view csv;
  typedef const char *ccs;
  typedef uint16_t SeqNum;
  typedef uint8_t Type;

private:
  struct rfc3550_hdr {
    /*
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
    uint8_t vpxcc;   // version, padding, extension, CSRC count
    uint8_t mpt;     // marker bit, payload type
    uint16_t seqnum; // sequence num
    union {
      struct {
        uint16_t timestmp;
        uint16_t ssrc;
      } rfc3350;

      uint32_t aad;
    } alt;

    uint8_t packetType() const { return mpt & ~0x80; }
  };

public:
  Packet() { reset(); }

  bool findFirstMarker();
  const rfc3550_hdr *header() const { return (rfc3550_hdr *)(this->data() + _marker_first_pos); }

  void loaded(size_t rx_bytes);

  ccs raw() const { return (ccs)data(); }
  void reset();

  SeqNum sequenceNum() const { return _seq_num; }
  uint32_t timestamp() const { return _timestamp; }
  Type type() const { return _type; }

  bool valid() const { return _valid; }
  const csv view() const { return csv(raw(), size()); }

  // misc helpers, debug, etc.
  void dumpHeader();

  static ccs fnName(src_loc loc = src_loc::current()) { return loc.function_name(); }

private:
  SeqNum _seq_num = 0;
  Type _type = 0;
  uint32_t _timestamp = 0;
  bool _valid = false;

  size_t _rx_bytes;
  size_t _marker_first_pos = 0;

  size_t _audio_begin_offset = 0;
  size_t _audio_len = 0;

  static constexpr size_t STD_PACKET_SIZE = 2048;
};

} // namespace audio
} // namespace tcp
} // namespace rtp
} // namespace pierre