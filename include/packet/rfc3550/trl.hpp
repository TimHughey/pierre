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

#include <array>
#include <cstdint>
#include <source_location>
#include <string>
#include <vector>

namespace pierre {
namespace packet {
namespace rfc3550 {

class trl {
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

    notes:

     1.  Apple only provides eight (8) bytes of nonce (defined as a NonceMini
         in this file).

     2.  ChaCha requires a twelve (12) bytes of nonce.

     3.  to creata a ChaCha nonce from the Apple nonce the first four (4) bytes
         are zeroed

 */

public:
  using string = std::string;
  typedef const char *ccs;
  typedef const unsigned char *cuc;

  typedef std::array<uint8_t, 12> Nonce;
  typedef std::array<uint8_t, 8> NonceMini;
  typedef std::array<uint8_t, 12> Tags;

private:
  Nonce nonce; // Apple uses eight (8) bytes
  Tags tags;   // Tags include crypto info for ChaCha

private:
  using src_loc = std::source_location;

public:
  trl() { clear(); } // empty, allow for placeholders
  trl(const std::vector<uint8_t> &src) { build(src.data(), src.size()); }
  trl(const uint8_t *src, size_t len) { build(src, len); }

  // struct operations
  void clear();
  static constexpr size_t size() { return sizeof(NonceMini); }

  // validation determined by observation
  bool isValid() const;

  // getters
  cuc noncePtr() const { return (cuc)nonce.data(); }
  size_t nonceLen() const { return nonce.size(); }

  // misc debug
  void dump(const src_loc loc = src_loc::current()) const;
  const string dumpString() const;

private:
  ccs fnName(const src_loc loc = src_loc::current()) const { return loc.function_name(); }
  void build(const uint8_t *src, size_t len);
  const string makeIndexByteString(uint8_t *bytes, size_t len) const;
};

} // namespace rfc3550
} // namespace packet
} // namespace pierre