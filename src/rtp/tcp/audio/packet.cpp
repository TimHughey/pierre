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

#include "rtp/tcp/audio/packet.hpp"

namespace pierre {
namespace rtp {
namespace tcp {
namespace audio {

bool Packet::findFirstMarker() {
  bool found = false;
  _marker_first_pos = 0;

  auto _begin_ = begin();
  auto _end_ = begin() + _rx_bytes;

  // note: we're looking at two bytes each pass so advance iterator by 2
  for (auto byte = _begin_; !found && (byte < _end_); std::advance(byte, 2)) {
    // check for RTP v2
    if (*byte & 0x40) {
      // find the marker bit
      if (*(byte + 1) & 0x40) {
        uint8_t type = *(byte + 1) & ~0x80;
        // find a packet type we want
        if ((type == 0x60) || (type == 0x56)) {
          // found it, set marker_pos
          _marker_first_pos = std::distance(begin(), byte);
          found = true;
        }
      }
    }
  }

  if (!found) {
    fmt::print(FMT_STRING("{} marker not found size={}\n"), fnName(), size());
  }

  return found;
}

void Packet::loaded([[maybe_unused]] size_t rx_bytes) {
  _rx_bytes = rx_bytes;

  // fmt::print(FMT_STRING("{} rx_bytes={:>6}\n"), fnName(), rx_bytes);

  // we're going to do some byte level stuff
  uint8_t *pkt = this->data();

  // capture the original packet size
  // we will adjust the length to define the audio portion
  _audio_len = this->size();

  // the packet type is at index 1
  _type = pkt[1] & ~0x80;

  if ((_type == 0x60) || (_type == 0x56)) {
    uint8_t *pktp = pkt;

    if (_type == 0x56) {
      // for packet types 0x56 move the iterator forware by 4
      pktp += 4;

      // also reduce the audio portion size by 4
      _audio_len -= 4;
    }

    // convert from network byte ordering
    _seq_num = ntohs(*(uint16_t *)(pktp + 2));

    // convert the timestamp from network by order
    _timestamp = ntohl(*(uint32_t *)(pktp + 4));

    // finalize the audio portion start and length
    _audio_begin_offset = std::distance(pkt, pktp) + 12;
    _audio_len -= 12;

    _valid = true;
  }
}

void Packet::reset() {
  clear();
  resize(STD_PACKET_SIZE);

  _seq_num = 0;
  _type = 0;
  _valid = false;
  _audio_begin_offset = 0;
  _audio_len = 0;
  _marker_first_pos = 0;
}

// misd helpers, debug, etc.

void Packet::dumpHeader() {
  const rfc3550_hdr *hdr = header();

  auto fmt_str = FMT_STRING(
      "{} marker={:>04}  vpxcc={:08b}  mpt={:08b}  pt={:#04x}  seqnum={:>8}  tstp={:>6}\n");

  fmt::print(fmt_str, fnName(), _marker_first_pos, hdr->vpxcc, hdr->mpt, hdr->packetType(),
             hdr->seqnum, hdr->alt.rfc3350.timestmp);
}

} // namespace audio
} // namespace tcp
} // namespace rtp
} // namespace pierre