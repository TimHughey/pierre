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

namespace pierre {
namespace rtp {
namespace tcp {
namespace audio {

struct rfc3550_trl {
  /*
    credit to https://emanuelecozzi.net/docs/airplay2/rtp packet info

          RFC 3550 Trailer
            0                   1                   2                   3
            0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
            :                                                               :
            |---------------------------------------------------------------|
    N-0x18 |                                                               |
            |--                          Nonce                            --|
    N-0x14 |                                                               |
            |---------------------------------------------------------------|
    N-0x10 |                                                               |
            |--                                                           --|
    N-0xc  |                                                               |
            |--                           Tag                             --|
    N-0x8  |                                                               |
            |--                                                           --|
    N-0x4  |                                                               |
            ---------------------------------------------------------------
    N

 */

  // the purpose of this struct is to provide structure to raw uint8_t
  // data. it is essential the member variables below remain in this
  // specific order and additional class members are not added

  // MSB ordering when applied to end of frame - sizeof(rfc3550_trl)
  uint8_t nonce[4]; // nonce for payload decryption
  uint8_t tag[16];  // tag for ChaCha20-Poly1305 verification

public:
  static rfc3550_trl *from(uint8_t *data, size_t len) {
    return (rfc3550_trl *)(data - len - size());
  }

  static constexpr size_t size() { return sizeof(rfc3550_trl); }
};

} // namespace audio
} // namespace tcp
} // namespace rtp
} // namespace pierre
