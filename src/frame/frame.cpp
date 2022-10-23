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
#include "base/threads.hpp"
#include "base/uint8v.hpp"
// #include "config/config.hpp"
#include "dsp.hpp"
#include "fft.hpp"
#include "master_clock.hpp"
#include "peaks.hpp"
#include "racked.hpp"
#include "types.hpp"

#include <cstdint>
#include <cstring>
#include <exception>
#include <iterator>
#include <latch>
#include <mutex>
#include <ranges>
#include <sodium.h>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

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
Frame::Frame(uint8v &packet) noexcept           //
    : created_at(pet::now_monotonic()),         // unique created time
      lead_time(InputInfo::lead_time),          // lead time used to calc state
      state(frame::HEADER_PARSED),              // frame header parsed
      version((packet[0] & 0b11000000) >> 6),   // RTPv2 == 0x02
      padding((packet[0] & 0b00100000) >> 5),   // has padding
      extension((packet[0] & 0b00010000) >> 4), // has extension
      ssrc_count((packet[0] & 0b00001111)),     // source system record count
      seq_num(packet.to_uint32(1, 3)),          // note: only three bytes
      timestamp(packet.to_uint32(4, 4)),        // RTP timestamp
      ssrc(packet.to_uint32(8, 4)),             // source system record count
      m(new cipher_buff_t)                      // unique_ptr for deciphered data
{}

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

  if (SharedKey::empty() == false) {
    cipher_rc = crypto_aead_chacha20poly1305_ietf_decrypt( // -1 == failure
        av::m_buffer(m),                                   // m (av leaves room for ADTS)
        &decipher_len,                                     // bytes deciphered
        nullptr,                                           // nanoseconds (unused, must be nullptr)
        ciphered.data(),                                   // ciphered data
        ciphered.size(),                                   // ciphered length
        aad.data(),                                        // authenticated additional data
        aad.size(),                                        // authenticated additional data length
        nonce.data(),                                      // the nonce
        SharedKey::key());                                 // shared key (from SETUP message)

    if ((cipher_rc >= 0) && decipher_len) {
      state = frame::DECIPHERED;

    } else if (decipher_len == 0) {
      state = frame::EMPTY;
    } else {
      state = frame::ERROR;
    }
  } else {
    state = frame::NO_SHARED_KEY;
  }

  log_decipher();

  return state == frame::DECIPHERED;
}

bool Frame::decode() {
  // NOTE:  this function is only called when the frame has been deciphered
  av::parse(shared_from_this());

  return state.dsp_any();
}

// Frame static functions for init
void Frame::init() {
  av::init();
  MasterClock::init();
  Anchor::init();
  Racked::init();
  dsp::init();

  INFO(Frame::module_id, "INIT", "frame sizeof={} lead_time={} fps={}\n", sizeof(Frame),
       pet::humanize(InputInfo::lead_time), InputInfo::fps);
}

frame::state Frame::state_now(AnchorLast anchor, const Nanos &lead_time) {
  _anchor.emplace(std::move(anchor)); // cache the anchor used for this calculation

  std::optional<frame::state> new_state;
  auto diff = _anchor->frame_local_time_diff(timestamp);

  if (diff < Nanos::zero()) {
    // first handle any outdated frames regardless of state
    new_state.emplace(frame::OUTDATED);
    diff = Nanos::zero(); // set sync wait to zero so caller can optimize timers

  } else if (state.updatable()) {
    // calculate the new state for READY, FUTURE or DSP_COMPLETE frames
    if ((diff >= Nanos::zero()) && (diff <= lead_time)) {

      // in range
      new_state.emplace(frame::READY);
    } else if ((diff > lead_time) && (state == frame::DSP_COMPLETE)) {

      // future
      new_state.emplace(frame::FUTURE);
    }
  } else if (state.dsp_incomplete()) {
    // dsp not finished, set reduced sync wait
    set_sync_wait(pet::percent(InputInfo::lead_time, 33));
  } else {
    INFO(module_id, "STATE_NOW", "unhandled state frame={}\n", inspect());
  }

  // when a new state is determined, update the local state and sync_wait
  if (new_state.has_value()) {
    state = new_state.value();
    set_sync_wait(diff);
  }

  return state;
}

Nanos Frame::sync_wait_recalc() {
  if (_anchor.has_value() && state.updatable()) {
    return set_sync_wait(_anchor->frame_local_time_diff(timestamp));
  } else if (_anchor.has_value()) {
    return sync_wait();
  }

  throw std::runtime_error("Frame::sync_wait_recalc() - no anchor\n");
}

// misc debug
const string Frame::inspect(bool full) {
  string msg;
  auto w = std::back_inserter(msg);

  if (full == true) {
    fmt::format_to(w, "vsn={} pad={} ext={} ssrc={} ", version, padding, extension, ssrc_count);
  }

  fmt::format_to(w, "seq_num={:<8} ts={:<12} {:<10} ready={:<5} sync_wait={}", //
                 seq_num, timestamp, state, state.ready(),
                 _sync_wait.has_value() ? pet::humanize(_sync_wait.value()) : "<no value>");

  // if (frame->silent()) {
  //   fmt::format_to(w, "silence=true");
  // }

  return msg;
}

const string Frame::inspect_safe(frame_t frame, bool full) { // static
  string msg;
  auto w = std::back_inserter(msg);

  fmt::format_to(w, "use_count={:02}", frame.use_count());

  if (frame.use_count() > 0) { // shared ptr not empty
    fmt::format_to(w, " {}", frame->inspect(full));
  }

  return msg;
}

void Frame::log_decipher() const {
  if (state.deciphered()) {
    INFOX(module_id, "DECIPHER", "decipher/cipher{:>6} / {:<6} {}\n", module_id, decipher_len,
          decoded.size(), state);

  } else if (state == frame::NO_SHARED_KEY) {
    INFO(module_id, "DECIPHER", "decipher shared key empty {}\n", state);
  } else {
    INFO(module_id, "DECIPHER", "decipher cipher_rc={} decipher_len={} {}\n", //
         state, cipher_rc, decipher_len);
  }
}

} // namespace pierre
