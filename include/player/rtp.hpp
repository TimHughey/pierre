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

#include "core/typedefs.hpp"
#include "packet/basic.hpp"
#include "player/flush_request.hpp"
#include "player/peaks.hpp"

#include <array>
#include <memory>
#include <utility>

namespace pierre {

namespace player {
typedef std::array<uint8_t, 16 * 1024> CipherBuff;
typedef std::shared_ptr<CipherBuff> shCipherBuff;

using Basic = packet::Basic;

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

class RTP : public std::enable_shared_from_this<RTP> {
private:
  RTP(Basic &packet);

public:
  RTP() = delete;
  static std::shared_ptr<RTP> create(Basic &packet);

  void cleanup() { _m.reset(); }

  bool decipher();                                        // sodium decipher packet
  static void decode(std::shared_ptr<RTP> rtp_packet);    // definition must be in cpp
  static void findPeaks(std::shared_ptr<RTP> rtp_packet); // defnition must be in cpp

  bool isReady() const { return _peaks_left.use_count() && _peaks_right.use_count(); }
  bool isValid() const { return version == 0x02; }
  bool keep(FlushRequest &flush);
  Basic &payload() { return _payload; }
  size_t payloadSize() const { return _payload.size(); }
  shPeaks peaksLeft() { return _peaks_left; }
  shPeaks peaksRight() { return _peaks_right; }

  // class member functions
  static void shk(const Basic &key); // set class level shared key
  static void shkClear();
  friend void swap(RTP &a, RTP &b); // swap specialization

  // misc debug
  void dump(bool debug = true) const;

public:
  // order dependent
  uint8_t version;
  bool padding;
  bool extension;
  uint8_t ssrc_count;
  uint32_t seq_num;
  uint32_t timestamp;
  uint32_t ssrc;

  // order independent
  size_t decipher_len = 0;
  bool decode_ok = false;
  shCipherBuff _m;

  int samples_per_channel = 0;
  int channels = 0;

private:
  // order independent
  packet::Basic _nonce;
  packet::Basic _tag;
  packet::Basic _aad;
  packet::Basic _payload;
  shPeaks _peaks_left;
  shPeaks _peaks_right;

  static constexpr auto moduleId = csv("RTP");
};

typedef std::shared_ptr<RTP> shRTP;

} // namespace player
} // namespace pierre