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

#include "packet/rtp.hpp"
#include "packet/basic.hpp"

#include "fmt/format.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <mutex>
#include <ranges>
#include <sodium.h>
#include <utility>

namespace pierre {
namespace packet {

namespace ranges = std::ranges;

namespace rtp {
uint32_t to_uint32(Basic::iterator begin, long int count) {
  uint32_t val = 0;
  size_t shift = (count - 1) * 8;

  for (auto it = std::counted_iterator{begin, count}; it != std::default_sentinel;
       ++it, shift -= 8) {
    val += *it << shift;
  }

  return val;
}

packet::Basic shk; // class level shared key
} // namespace rtp

namespace ranges = std::ranges;

RTP::RTP(Basic &packet) {
  Basic _packet;              // packet to pick apart
  std::swap(_packet, packet); // swap empty packet with packet passed

  uint8_t byte0 = _packet[0]; // first byte to pick bits from

  version = (byte0 & 0b11000000) >> 6;     // RTPv2 == 0x02
  padding = ((byte0 & 0b00100000) >> 5);   // has padding
  extension = ((byte0 & 0b00010000) >> 4); // has extension
  ssrc_count = ((byte0 & 0b00001111));     // source system record count

  seq_num = rtp::to_uint32(_packet.begin() + 1, 3);   // note: onlythree bytes
  timestamp = rtp::to_uint32(_packet.begin() + 4, 4); // four bytes
  ssrc = rtp::to_uint32(_packet.begin() + 8, 4);      // four bytes

  // grab the end of the packet, we need it for several parts
  Basic::iterator packet_end = _packet.begin() + _packet.size();

  // nonce is the last eight (8) bytes
  // a complete nonce is 12 bytes so zero pad then append the mini nonce
  _nonce.assign(4, 0x00);
  auto nonce_inserter = std::back_inserter(_nonce);
  ranges::copy_n(packet_end - 8, 8, nonce_inserter);

  // tag is 24 bytes from end and 16 bytes in length
  Basic::iterator tag_begin = packet_end - 24;
  Basic::iterator tag_end = tag_begin + 16;
  _tag.assign(tag_begin, tag_end);

  Basic::iterator aad_begin = _packet.begin() + 4;
  Basic::iterator aad_end = aad_begin + 8;
  _aad.assign(aad_begin, aad_end);

  // finally the actual payload, starts at byte 12, ends
  Basic::iterator payload_begin = _packet.begin() + 12;
  Basic::iterator payload_end = packet_end - 8;
  _payload.assign(payload_begin, payload_end);

  // NOTE: raw packet falls out of scope
}

/*
 * Many thanks to:
 * https://stackoverflow.com/questions/18862715/how-to-generate-the-aac-adts-elementary-stream-with-android-mediacodec
 *
 * Add ADTS header at the beginning of each and every AAC packet.
 * This is needed as MediaCodec encoder generates a packet of raw AAC data.
 *
 * NOTE: the payload size must count in the ADTS header itself.
 */

void RTP::adtsHeaderAdd() {
  int profile = 2; // AAC LC
                   // 39=MediaCodecInfo.CodecProfileLevel.AACObjectELD;
  int freqIdx = 4; // 44.1KHz
  int chanCfg = 2; // CPE

  // fill in ADTS data
  _payload[0] = 0xFF;
  _payload[1] = 0xF9;
  _payload[2] = ((profile - 1) << 6) + (freqIdx << 2) + (chanCfg >> 2);
  _payload[3] = ((chanCfg & 3) << 6) + (_payload.size() >> 11);
  _payload[4] = (_payload.size() & 0x7FF) >> 3;
  _payload[5] = ((_payload.size() & 7) << 5) + 0x1F;
  _payload[6] = 0xFC;
}

bool RTP::decipher() {
  if (rtp::shk.empty()) {
    if (true) { // debug
      constexpr auto f = FMT_STRING("{} {} shk empty\n");
      fmt::print(f, runTicks(), fnName());
    }

    return false;
  }

  std::array<uint8_t, 16 * 1024> m; // deciphered data destination
  unsigned long long decip_len = 0; // deciphered length
  auto cipher_rc =                  // returns -1 == failure
      crypto_aead_chacha20poly1305_ietf_decrypt(
          m.data(),                          // m (decipered data)
          &decip_len,                        // bytes deciphered
          nullptr,                           // nanoseconds (unused, must be nullptr)
          _payload.data(),                   // ciphered data
          _payload.size(),                   // ciphered length
          _aad.data(),                       // authenticated additional data
          _aad.size(),                       // authenticated additional data length
          _nonce.data(),                     // the nonce
          (const uint8_t *)rtp::shk.data()); // shared key (from SETUP message)

  if (cipher_rc < 0) { // crypto returns -1 on failure
    static std::once_flag __flag;

    std::call_once(__flag, [&] {
      constexpr auto f = FMT_STRING("{} {} failed rc={} len={}\n");
      fmt::print(f, runTicks(), fnName(), cipher_rc, decip_len);
    });
  }

  if (false) { // debug
    constexpr auto f = FMT_STRING("{} {} OK rc={} decipher/cipher{:>6}/{:>6}\n");
    fmt::print(f, runTicks(), fnName(), cipher_rc, decip_len, _payload.size());
  }

  // replace payload with deciphered data
  // _payload.assign(7, 0x00); // reserve space for adts header
  // auto to = std::back_inserter(_payload);
  // ranges::copy_n(m.begin(), decip_len, to);

  if (_payload.size() < 64) {
    constexpr auto f = FMT_STRING("{} {} small payload size={}\n");
    fmt::print(f, runTicks(), fnName(), _payload.size());

    fmt::print("{} ", runTicks());
    size_t count = 0;
    ranges::for_each(_payload.begin(), _payload.end(), [&](const auto &byte) {
      fmt::print("[{:<02}]0x{:<02x}  ", count, byte);
      ++count;

      if ((count % 10) == 0) {
        fmt::print("\n{} ", runTicks());
      }
    });

    if ((count % 10) != 0) {
      fmt::print("\n");
    }
  }

  return true;
}

void RTP::shk(const Basic &key) {
  rtp::shk = key;

  if (false) { // debug
    constexpr auto f = FMT_STRING("{} {} size={}\n");
    fmt::print(f, runTicks(), fnName(), rtp::shk.size());
  }
}

} // namespace packet
} // namespace pierre