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
#include "base/pet.hpp"
#include "base/stats.hpp"
#include "base/uint8v.hpp"
#include "fft.hpp"

#include <iterator>
#include <ranges>
#include <sodium.h>
#include <span>
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

bool Frame::decipher(uint8v packet, const uint8v key) noexcept {

  // ensure we know how big of a cipher buffer to allocate
  // if (!cipher_buff_size) {
  //   conf::token ctoken(module_id);

  //   cipher_buff_size = ctoken.val<ptrdiff_t, toml::table>("cipher.buffer_size"_tpath, 0x4000L);
  // }

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

    Stats::write(stats::RTSP_AUDIO_CIPHERED, std::ssize(packet));

    unsigned long long consumed{0};

    cipher_rc =                                    //
        crypto_aead_chacha20poly1305_ietf_decrypt( // -1 == failure
            Av::m_buffer(m, cipher_buff_size),     // m (av leaves room for ADTS)
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

      Stats::write(stats::RTSP_AUDIO_DECIPERED, std::ssize(*m));
      state = frame::DECIPHERED;

    } else if (cipher_rc < 0) {
      state = frame::DECIPHER_FAILURE;

    } else if (consumed <= 0) {
      state = frame::EMPTY;

    } else { // catch all
      state = frame::ERROR;
    }
  }

  log_decipher();

  return state == frame::DECIPHERED;
}

bool Frame::decode() noexcept {
  if (!av) av = std::make_unique<Av>();

  return state.deciphered() && av->parse(*this) && state.dsp_any();
}

frame::state Frame::state_now(const AnchorLast anchor) noexcept {
  if (synthetic()) {
    // synthetic frames are always ready
    state = frame::READY;
  } else if (anchor.ready()) {
    // cache the anchor used for this calculation
    _anchor.emplace(std::move(anchor));

    auto diff = _anchor->frame_local_time_diff(timestamp);

    state = state_now(diff, InputInfo::lead_time);
  }

  return state;
}

frame::state Frame::state_now(const Nanos diff, const Nanos &lead_time) noexcept {

  // save the initial value of the state to confirm we don't conflict with another
  // thread (e.g. dsp processing) that may also change the state
  std::optional<frame::state> new_state;

  if (synthetic()) {
    new_state.emplace(frame::READY);

  } else if (diff < Nanos::zero()) {
    // first handle any outdated frames regardless of state
    new_state.emplace(frame::OUTDATED);

  } else if (state.updatable()) {
    // calculate the new state for READY, FUTURE or DSP_COMPLETE frames
    if ((diff >= Nanos::zero()) && (diff <= lead_time)) {

      // in range
      new_state.emplace(frame::READY);
    } else if ((diff > lead_time) && (state == frame::DSP_COMPLETE)) {

      // future
      new_state.emplace(frame::FUTURE);
    }
  }

  set_sync_wait(diff);

  Stats::write(stats::SYNC_WAIT, sync_wait(), state.tag());

  return state;
}

Nanos Frame::sync_wait_recalc() noexcept {
  Nanos updated;

  if (_anchor.has_value()) {
    updated = set_sync_wait(_anchor->frame_local_time_diff(timestamp));
  } else if (synthetic()) {
    updated = sync_wait();
  } else {
    INFO(module_id, "SYNC_WAIT", "anchor={}\n", _anchor.has_value());
    updated = sync_wait();
  }

  return updated;
}

void Frame::log_decipher() const noexcept {
  static constexpr csv fn_id{"decipher"};
  auto consumed = m.has_value() ? std::ssize(*m) : 0;

  if (state.deciphered()) {
    INFO_AUTO("consumed={:>6} / {:<6} {}\n", consumed, std::ssize(*m), state);

  } else if (state == frame::NO_SHARED_KEY) {
    INFO_AUTO("shared key empty {}\n", state);
  } else {
    INFO_AUTO("cipher_rc={} consumed={} {}\n", cipher_rc, consumed, state);
  }
}

// class data
ptrdiff_t Frame::cipher_buff_size{0x2000};

} // namespace pierre
