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
#include "av.hpp"
#include "base/anchor_last.hpp"
#include "base/elapsed.hpp"
#include "base/input_info.hpp"
#include "base/io.hpp"
#include "base/shk.hpp"
#include "base/uint8v.hpp"
#include "dsp.hpp"
#include "fft.hpp"
#include "master_clock.hpp"
#include "racked.hpp"

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

// Frame API
Frame::Frame()
    : created_at(pet::now_monotonic()), // unique created time
      state(frame::EMPTY)               // flag frame as empty
{}

Frame::Frame(uint8v &packet) noexcept           //
    : created_at(pet::now_monotonic()),         // unique created time
      state(frame::HEADER_PARSED),              // frame header parsed
      version((packet[0] & 0b11000000) >> 6),   // RTPv2 == 0x02
      padding((packet[0] & 0b00100000) >> 5),   // has padding
      extension((packet[0] & 0b00010000) >> 4), // has extension
      ssrc_count((packet[0] & 0b00001111)),     // source system record count
      ssrc(packet.to_uint32(8, 4)),             // source system record count
      seq_num(packet.to_uint32(1, 3)),          // note: only three bytes
      timestamp(packet.to_uint32(4, 4)),        // RTP timestamp
      m(new cipher_buff_t)                      // unique_ptr for deciphered data
{}

Frame::Frame(Nanos wait) noexcept : created_at(pet::now_monotonic()) { set_sync_wait(wait); }

bool Frame::decipher(uint8v &packet) noexcept {

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

  if (SharedKey::empty()) {
    state = frame::NO_SHARED_KEY;

  } else if (version != RTPv2) {
    state = frame::INVALID;

  } else [[likely]] {

    cipher_rc =                                    //
        crypto_aead_chacha20poly1305_ietf_decrypt( // -1 == failure
            av::m_buffer(m),                       // m (av leaves room for ADTS)
            &decipher_len,                         // bytes deciphered
            nullptr,                               // nanoseconds (unused, must be nullptr)
            ciphered.data(),                       // ciphered data
            ciphered.size(),                       // ciphered length
            aad.data(),                            // authenticated additional data
            aad.size(),                            // authenticated additional data length
            nonce.data(),                          // the nonce
            SharedKey::key());                     // shared key (from SETUP message)

    if ((cipher_rc >= 0) && (decipher_len > 0)) {
      state = frame::DECIPHERED;

    } else if (cipher_rc < 0) {
      state = frame::DECIPHER_FAILURE;

    } else if (decipher_len <= 0) {
      state = frame::EMPTY;

    } else { // catch all
      state = frame::ERROR;
    }
  }

  log_decipher();

  return state == frame::DECIPHERED;
}

bool Frame::decode() noexcept {
  return state.deciphered() && av::parse(shared_from_this()) && state.dsp_any();
}

// Frame static functions for init
void Frame::init() {
  av::init();
  MasterClock::init();
  Anchor::init();
  Racked::init();
  dsp::init();

  INFO(Frame::module_id, "INIT", "sizeof={} lead_time={} fps={}\n", sizeof(Frame),
       pet::humanize(InputInfo::lead_time), InputInfo::fps);
}

frame::state Frame::state_now(AnchorLast anchor, const Nanos &lead_time) noexcept {
  if (anchor.ready()) {
    _anchor.emplace(std::move(anchor)); // cache the anchor used for this calculation

    auto diff = _anchor->frame_local_time_diff(timestamp);

    state = state_now(diff, lead_time);
  }

  return state;
}

frame::state Frame::state_now(const Nanos diff, const Nanos &lead_time) noexcept {

  // save the initial value of the state to confirm we don't conflict with another
  // thread (e.g. dsp processing) that may also change the state
  auto initial_state = state.now();
  std::optional<frame::state> new_state;

  if (diff < Nanos::zero()) {
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

  // if the new state was calculated apply it only if the initial state hasn't changed
  // this is necessary because we are now running in a thread environment
  state.update_if(initial_state, new_state);
  set_sync_wait(diff);

  return state;
}

Nanos Frame::sync_wait_recalc() noexcept {
  Nanos updated;

  if (_anchor.has_value()) {
    updated = set_sync_wait(_anchor->frame_local_time_diff(timestamp));
  } else {
    INFO(module_id, "ERROR", "sync_wait not recalculated anchor={}\n", _anchor.has_value());
    updated = sync_wait();
  }

  return updated;
}

// misc debug
const string Frame::inspect(bool full) const noexcept {
  string msg;
  auto w = std::back_inserter(msg);

  if (full == true) {
    fmt::format_to(w, "vsn={} pad={} ext={} ssrc={} ", version, padding, extension, ssrc_count);
  }

  fmt::format_to(w, "seq_num={:<8} ts={:<12} {:<10}", seq_num, timestamp, state);

  if (sync_wait_ok_safe()) {
    fmt::format_to(w, " sync_wait={}", pet::humanize(sync_wait()));
  }

  if (decipher_len <= 0) {
    fmt::format_to(w, " decipher_len={}", decipher_len);
  }

  // if (silent()) {
  //   fmt::format_to(w, " silence={}", true);
  // }

  return msg;
}

const string Frame::inspect_safe(frame_t frame, bool full) noexcept { // static
  string msg;
  auto w = std::back_inserter(msg);

  fmt::format_to(w, "use_count={:02}", frame.use_count());

  if (frame.use_count() > 0) { // shared ptr not empty
    fmt::format_to(w, " {}", frame->inspect(full));
  }

  return msg;
}

void Frame::log_decipher() const noexcept {
  if (state.deciphered()) {
    INFOX(module_id, "DECIPHER", "decipher/cipher{:>6} / {:<6} {}\n", module_id, decipher_len,
          decoded.size(), state);

  } else if (state == frame::NO_SHARED_KEY) {
    INFO(module_id, "DECIPHER", "decipher shared key empty {}\n", state);
  } else {
    INFOX(module_id, "DECIPHER", "decipher cipher_rc={} decipher_len={} {}\n", //
          cipher_rc, decipher_len, state);
  }
}

} // namespace pierre
