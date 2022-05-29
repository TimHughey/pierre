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
#include "packet/flush_request.hpp"

#include <array>
#include <memory>
#include <utility>

namespace pierre {

namespace packet {
typedef std::array<uint8_t, 16 * 1024> CipherBuff;

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
public:
  RTP() = default;
  RTP(Basic &packet); // must construct from a packet
                      //   RTP(const RTP &) = default;
                      //   RTP(RTP &&) = default;
                      //   RTP &operator=(const RTP &) = default;
                      //   RTP &operator=(RTP &&rtp) = default;

  //   RTP(const RTP &) = delete;
  //   RTP(RTP &&rtp) noexcept;
  //   RTP &operator=(const RTP &) = delete;
  //   RTP &operator=(RTP &&rhs) noexcept;

  friend void swap(RTP &a, RTP &b); // swap specialization

  bool decipher(); // deciphers and processes frame
  bool isValid() const { return version == 0x02; }
  bool keep(FlushRequest &flush);
  size_t payloadSize() const { return _payload.size(); }
  const Basic &pcmSamples() const { return _payload; }

  // class member functions
  static void shk(const Basic &key); // set class level shared key
  static void shkClear();

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

private:
  // order independent
  Basic _nonce;
  Basic _tag;
  Basic _aad;
  Basic _payload;

  static constexpr size_t ADTS_HEADER_SIZE = 7;
  static constexpr int ADTS_PROFILE = 2;     // AAC LC
  static constexpr int ADTS_FREQ_IDX = 4;    // 44.1 KHz
  static constexpr int ADTS_CHANNEL_CFG = 2; // CPE

  static constexpr auto moduleId = csv("RTP");
};

typedef std::shared_ptr<RTP> shRTP;

} // namespace packet
} // namespace pierre