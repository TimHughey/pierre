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
#include "config/config.hpp"
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

// Frame Class Data
static std::optional<Racked> _racked;

// Frame API
Frame::Frame(uint8v &packet) noexcept           //
    : created_at(pet::now_monotonic()),         // unique created time
      sync_wait(Nanos::min()),                  // init sync_wait to invalid
      lead_time(InputInfo::lead_time()),        // lead time used to calc state
      state(frame::EMPTY),                      // frame begins life empty
      version((packet[0] & 0b11000000) >> 6),   // RTPv2 == 0x02
      padding((packet[0] & 0b00100000) >> 5),   // has padding
      extension((packet[0] & 0b00010000) >> 4), // has extension
      ssrc_count((packet[0] & 0b00001111)),     // source system record count
      seq_num(packet.to_uint32(1, 3)),          // note: only three bytes
      timestamp(packet.to_uint32(4, 4)),        // RTP timestamp
      ssrc(packet.to_uint32(8, 4)),             // source system record count
      m(new cipher_buff_t)                      // unique_ptr for deciphered data
{
  // the nonce for libsodium is 12 bytes however the packet only provides 8
  uint8v nonce(4, 0x00); // pad the 12 byte nonce
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
}

// Frame static functions for init and shutdown
void Frame::init() {
  MasterClock::init();
  Anchor::init();
  Racked::init();
  dsp::init();
}

void Frame::shutdown() { dsp::shutdown(); }

// Frame API and member functions

state_now_result_t Frame::calc_sync_wait(AnchorLast &anchor) {
  auto diff = anchor.frame_local_time_diff(timestamp);

  // update sync_wait anything has changed
  if (diff != sync_wait) {
    // if (sync_wait != Nanos::min()) {
    //   __LOG(LCOL01 " seq={:>8} sync_wait={}/{}\n", module_id, "CALC_SYNC_WAIT", seq_num,
    //          pet::humanize(sync_wait), pet::humanize(diff));
    // }
    sync_wait = diff;
  }

  return std::make_tuple(sync_wait > Nanos::min(), sync_wait, state);
}

void Frame::find_peaks() {
  constexpr size_t SAMPLE_BYTES = sizeof(uint16_t); // bytes per sample

  FFT fft_left(samples_per_channel, InputInfo::rate);
  FFT fft_right(samples_per_channel, InputInfo::rate);

  // first things first, deinterlace the raw *uint8_t) payload into left/right reals (floats)
  auto &real_left = fft_left.real();
  auto &real_right = fft_right.real();

  uint8_t *sptr = decoded.data(); // ptr to next sample
  const auto interleaved_samples = std::size(decoded) / SAMPLE_BYTES;

  for (size_t idx = 0; idx < interleaved_samples; ++idx) {
    uint16_t val;
    std::memcpy(&val, sptr, SAMPLE_BYTES); // memcpy to avoid strict aliasing rules
    sptr += SAMPLE_BYTES;                  // increment to next sample

    if ((idx % 2) == 0) { // left channel
      real_left.emplace_back(static_cast<float>(val));
    } else { // right channel
      real_right.emplace_back(static_cast<float>(val));
    }
  }

  // now process each channel then save the peaks
  fft_left.process();
  fft_right.process();

  peaks = std::make_tuple(fft_left.find_peaks(), fft_right.find_peaks());

  uint8v empty;
  std::swap(decoded, empty); // indirectly free decoded

  state = frame::DSP_COMPLETE;
}

bool Frame::parse() {
  auto rc = false;

  if (state == frame::DECIPHERED) {
    av::parse(shared_from_this());
    rc = true;
  }

  return rc;
}

void Frame::process() {
  if (state == frame::DECODED) {
    dsp::process(shared_from_this());
  }
}

frame::state Frame::state_now(AnchorLast anchor, const Nanos &lead_time) {
  _anchor = std::move(anchor); // cache the anchor used for this calculation

  auto diff = _anchor.frame_local_time_diff(timestamp);

  // first handle any outdated frames regardless of state
  if (diff < Nanos::zero()) {
    // auto prev_state = state;
    state = frame::OUTDATED;
    sync_wait = Nanos::zero(); // set sync wait to zero so caller can optimize timers

    // __LOG0(LCOL01 " CHANGED {} prev_state={} diff={}\n", //
    //        module_id, "STATE_NOW", inspect(), prev_state(),
    //        diff != Nanos::min() ? pet::humanize(diff) : "<min>");
  }

  // calculate the new state for READY, FUTURE or DSP_COMPLETE frames
  if (state.updatable()) {
    if ((diff >= Nanos::zero()) && (diff <= lead_time)) { // in range
      state = frame::READY;

    } else if ((diff > lead_time) && (state == frame::DSP_COMPLETE)) { // future
      state = frame::FUTURE;
    }

    set_sync_wait(diff);

  } // end frame state update

  return state;
}

// misc debug
const string Frame::inspect(bool full) {
  string msg;
  auto w = std::back_inserter(msg);

  if (full == true) {
    fmt::format_to(w, "vsn={} pad={} ext={} ssrc={} ", version, padding, extension, ssrc_count);
  }

  fmt::format_to(w, "seq_num={:<8} ts={:<12} state={:<10} ready={}", //
                 seq_num, timestamp, state(), state.ready());

  fmt::format_to(w, " sync_wait={}",
                 sync_wait > Nanos::min() ? pet::humanize(sync_wait) : "<unset>");

  // if (frame->silent()) {
  //   fmt::format_to(w, " silence=true");
  // }

  return msg;
}

const string Frame::inspect_safe(frame_t frame, bool full) { // static
  string msg;
  auto w = std::back_inserter(msg);

  if (frame.use_count() == 0) { // shared ptr is empty
    fmt::format_to(w, "use_count={:02}", frame.use_count());
  } else {
    fmt::format_to(w, "use_count={:02}", frame.use_count());
    fmt::format_to(w, " {}", frame->inspect(full));
  }

  return msg;
}

void Frame::log_decipher() const {
  if (state == frame::DECIPHERED) {
    __LOGX(LCOL01 " decipher/cipher{:>6}/{:>6}\n", module_id, state(), decipher_len,
           decoded.size());

  } else if (state == frame::NO_SHARED_KEY) {
    __LOG0(LCOL01 " decipher shared key empty\n", module_id, state());
  } else {
    __LOG0(LCOL01 " decipher size={} cipher_rc={} decipher_len={}\n", //
           module_id, state(), decoded.size(), cipher_rc, decipher_len);
  }
}

} // namespace pierre
