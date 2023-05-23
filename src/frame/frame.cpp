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

#include "frame.hpp"
#include "anchor.hpp"
#include "anchor_last.hpp"
#include "av.hpp"
#include "base/elapsed.hpp"
#include "base/input_info.hpp"
#include "base/logger.hpp"
#include "base/pet.hpp"
#include "base/stats.hpp"
#include "base/uint8v.hpp"
#include "fft.hpp"
#include "frame/flusher.hpp"
#include "master_clock.hpp"

#include <fmt/chrono.h>
#include <iterator>
#include <sodium.h>
#include <utility>

namespace pierre {

/* credit to https://emanuelecozzi.net/docs/airplay2/rt for the packet info

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
     are zeroed */

std::unique_ptr<Av> Frame::av{nullptr};

// Frame API
Frame::Frame(Frame &&other) noexcept {
  state = std::exchange(other.state, frame::MOVED);
  version = other.version;
  padding = other.padding;
  extension = other.extension;
  ssrc_count = other.ssrc_count;
  ssrc = other.ssrc;
  seq_num = std::exchange(other.seq_num, 0);
  timestamp = std::exchange(other.timestamp, 0);

  samp_per_ch = std::exchange(other.samp_per_ch, 0);
  channels = std::exchange(other.channels, 0);
  rand_gen = std::move(rand_gen);
  cached_sync_wait = std::move(other.cached_sync_wait);
  silence = other.silence;
  peaks = std::move(other.peaks);
}

Frame::~Frame() noexcept {}

Frame &Frame::operator=(Frame &&lhs) noexcept {

  if (sn() == lhs.sn()) return *this;

  state = std::exchange(lhs.state, frame::MOVED);
  version = lhs.version;
  padding = lhs.padding;
  extension = lhs.extension;
  ssrc_count = lhs.ssrc_count;
  ssrc = lhs.ssrc;
  seq_num = std::exchange(lhs.seq_num, 0);
  timestamp = std::exchange(lhs.timestamp, 0);

  samp_per_ch = std::exchange(lhs.samp_per_ch, 0);
  channels = std::exchange(lhs.channels, 0);
  rand_gen = std::move(rand_gen);
  cached_sync_wait = std::move(lhs.cached_sync_wait);
  silence = lhs.silence;
  peaks = std::move(lhs.peaks);

  return *this;
}

frame_state_v Frame::decipher(uint8v packet, const uint8v key) noexcept {
  INFO_AUTO_CAT("decipher");

  constexpr uint8_t RTPv2{0x02};

  // the nonce for libsodium is 12 bytes however the packet only provides 8
  uint8v nonce(4, 0x00); // pad the nonce for libsodium
  // mini nonce end - 8; copy 8 bytes total
  ranges::copy_n(packet.from_end(8), 8, std::back_inserter(nonce));

  // tag end - 24 bytes; 16 bytes total
  std::span<uint8_t> tag(packet.from_end(24), 16);

  // aad begin + 4 bytes; 8 bytes total
  std::span<uint8_t> aad(packet.from_begin(4), 8);

  // ciphered data begin + 12, end - 8
  std::span<uint8_t> ciphered(packet.from_begin(12), packet.from_end(8));

  if (key.empty()) {
    state = frame::NO_SHARED_KEY;

  } else if (version != RTPv2) {
    state = frame::INVALID;

  } else [[likely]] {
    int cipher_rc{0};
    ull_t consumed{0};
    auto m = Av::make_m_buffer();

    Stats::write<Frame>(stats::RTSP_AUDIO_CIPHERED, std::ssize(packet));

    cipher_rc =                                    //
        crypto_aead_chacha20poly1305_ietf_decrypt( // -1 == failure
            Av::m_buffer(m),                       // m (av leaves room for ADTS)
            &consumed,                             // bytes deciphered
            nullptr,                               // nanoseconds (unused, must be nullptr)
            ciphered.data(),                       // ciphered data
            ciphered.size(),                       // ciphered length
            aad.data(),                            // authenticated additional data
            aad.size(),                            // authenticated additional data length
            nonce.data(),                          // the nonce
            key.data());                           // shared key (from SETUP message)

    if ((cipher_rc >= 0) && (consumed > 0)) {
      Av::m_buffer_resize(m, consumed); // resizes m to include header + cipher data

      Stats::write<Frame>(stats::RTSP_AUDIO_DECIPERED, consumed);

      state = frame::DECIPHERED;

    } else if (cipher_rc < 0) {
      state = frame::DECIPHER_FAIL;

    } else if (consumed <= 0) {
      state = frame::NO_AUDIO;

    } else { // catch all
      state = frame::ERROR;
    }

    log_decipher(cipher_rc, consumed, packet, m);

    if (state == frame::DECIPHERED) {
      if (!av) av = std::make_unique<Av>();

      return av->decode(*this, std::move(m));
    }
  }

  return (frame_state_v)state.record_state();
}

void Frame::init() noexcept {
  const auto lead_time = pet::humanize(InputInfo::lead_time);
  INFO_INIT("sizeof={:>5} lead_time={}", sizeof(Frame), lead_time);
}

void Frame::log_decipher(int cipher_rc, ull_t consumed, const uint8v &p,
                         const uint8v &m) const noexcept {
  INFO_AUTO_CAT("decipher");

  switch ((frame_state_v)state) {
  case frame::DECIPHERED:
    INFO_AUTO("OK consumed={:>6} / {:<6}", consumed, m.size());
    break;

  case frame::NO_SHARED_KEY:
    INFO_AUTO("{}", state);
    break;

  case frame::NO_AUDIO:
    INFO_AUTO("no audio? p[0]={:#02x} size={}", p[0], p.size());
    break;

  default:
    INFO_AUTO("decipher failed, cipher_rc={}", cipher_rc);
  }
}

void Frame::flush() noexcept {
  state = frame::FLUSHED;
  record_state();
}

bool Frame::flush(Flusher &flusher) noexcept {
  auto rc = flusher.check(*this);

  if (rc) flush();

  return rc;
}

void Frame::record_sync_wait() const noexcept {
  // write diffs of interest to the timeseries database
  Stats::write<Frame>(stats::SYNC_WAIT, sync_wait(), state.tag());
}

frame::state Frame::state_now(AnchorLast &ancl) noexcept {
  INFO_AUTO_CAT("state_now");

  if (can_render()) {
    if (ancl.ready()) {
      auto diff = ancl.frame_local_time_diff(*this);

      if (diff <= 0ns) {
        state = frame::OUTDATED;
        cache_sync_wait(diff);
      } else if ((diff > 0ns) && (diff <= InputInfo::lead_time)) {
        state = frame::READY;
        cache_sync_wait(diff);
      } else if (diff > InputInfo::lead_time) {
        state = frame::FUTURE;
        cache_sync_wait(diff);
      } else {
        INFO_AUTO("NO CHANGE {}", *this);
      }

    } else {
      state = frame::NO_CLK_ANC;
      cache_sync_wait(rand_gen(50000us, 120000us));
    }
  }

  return state;
}

bool Frame::sync_wait(MasterClock &clk, Anchor &anchor, ftime_t ft, Nanos &sync_wait) noexcept {

  if (ft > 0) {
    auto clk_info = clk.info();

    if (auto ancl = anchor.get_data(clk_info); ancl.ready()) {
      sync_wait = ancl.frame_local_time_diff(ft);

      return true;
    }
  }

  return false;
}

} // namespace pierre
