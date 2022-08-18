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

#include "base/elapsed.hpp"
#include "base/flush_request.hpp"
#include "base/input_info.hpp"
#include "base/pe_time.hpp"
#include "base/typical.hpp"
#include "base/uint8v.hpp"
#include "frame/peaks.hpp"

#include <any>
#include <array>
#include <map>
#include <memory>
#include <ranges>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace ranges = std::ranges;
}

namespace pierre {
namespace fra {

typedef string_view State;
typedef csv StateConst;
typedef std::vector<State> States;
typedef std::vector<State> StateKeys;
typedef std::map<State, size_t> StatsMap;

constexpr csv ANCHOR_DELAY{"anchor_delay"};
constexpr csv DECODED{"decoded"};
constexpr csv EMPTY{"empty"};
constexpr csv ERROR{"error"};
constexpr csv FUTURE{"future"};
constexpr csv INVALID{"invalid"};
constexpr csv NONE{"none"};
constexpr csv NOT_READY{"not_ready"};
constexpr csv OUTDATED{"outdated"};
constexpr csv PLAYABLE{"playable"};
constexpr csv PLAYED{"played"};

StateKeys &state_keys();

void start_dsp_threads();
} // namespace fra

typedef std::array<uint8_t, 16 * 1024> CipherBuff;
typedef std::shared_ptr<CipherBuff> shCipherBuff;
typedef unsigned long long ullong_t;

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

class Frame;
typedef std::shared_ptr<Frame> shFrame;

class Frame : public std::enable_shared_from_this<Frame> {

private:
  Frame(uint8v &packet);
  Frame(uint8_t version, fra::State state, bool silence)
      : version(version), _state(state), peaks_left(Peaks::create()), peaks_right(Peaks::create()),
        _silence(silence) {}

public:
  // Object Creation
  static shFrame create(uint8v &packet) { return shFrame(new Frame(packet)); }
  static shFrame createSilence() { return shFrame(new Frame(0x02, fra::PLAYABLE, true)); }

  // Public API

  Nanos calc_sync_wait(Nanos const &lag, bool log = false) {
    sync_wait = localTimeDiff() - lag;
    sync_wait = (sync_wait < Nanos::zero()) ? Nanos::zero() : sync_wait;

    if (log) {
      __LOG0(LCOL01 " seq_num={} sync_wait={:0.2}\n", //
             moduleID(), "CALC_SYNC", seq_num,        //
             pe_time::as_duration<Nanos, MillisFP>(sync_wait));
    }

    return sync_wait;
  }

  bool decipher(); // sodium decipher packet
  void decodeOk() { _state = fra::DECODED; }
  static shFrame ensureFrame(shFrame frame) { return ok(frame) ? frame : createSilence(); }
  static void findPeaks(shFrame frame); // defnition must be in cpp
  bool isReady() const { return peaks_left.use_count() && peaks_right.use_count(); }
  bool isValid() const { return version == 0x02; }

  bool keep(FlushRequest &flush);

  // NOTE: localTime() and localTimeDiff() must be defined in .cpp due to intentional
  // limited visibility of Anchor
  const Nanos localTimeDiff(); // calculate diff between local and frame time
  void mark_played() { _state = fra::PLAYED; }

  // nextFrame() returns true when searching should stop; false to keep searching
  bool nextFrame(const Nanos &lead_time);

  uint8v &payload() { return _payload; }
  size_t payloadSize() const { return _payload.size(); }

  // cached frame time info
  template <typename T> const T nettime() {
    return pe_time::as_duration<Nanos, T>(cached_nettime);
  }

  template <typename T> const T time_diff() {
    return pe_time::as_duration<Nanos, T>(cached_time_diff);
  }

  // state
  bool decoded() const { return stateEqual(fra::DECODED); }
  static bool empty(shFrame frame) { return ok(frame) && frame->stateEqual(fra::EMPTY); }
  bool future() const { return stateEqual(fra::FUTURE); }
  static bool ok(shFrame frame) { return frame.use_count(); }
  bool outdated() const { return stateEqual(fra::OUTDATED); }
  bool playable() const { return stateEqual({fra::PLAYABLE, fra::FUTURE}); }
  bool played() const { return stateEqual(fra::PLAYED); }
  bool purgeable() const { return stateEqual({fra::OUTDATED, fra::PLAYED}); }
  bool silence() const { return _silence; }
  static fra::StateConst &stateVal(shFrame frame) {
    return frame.use_count() ? frame->_state : fra::EMPTY;
  }
  bool stateEqual(fra::StateConst &check) const { return check == _state; }
  bool stateEqual(const fra::States &states) const {
    return ranges::any_of(states, [&](const auto &state) { return stateEqual(state); });
  }

  bool unplayed() const { return stateEqual({fra::DECODED, fra::FUTURE, fra::PLAYABLE}); }

  // class member functions
  friend void swap(Frame &a, Frame &b); // swap specialization

  // misc debug
  const string inspect() { return inspectFrame(shared_from_this()); }
  static const string inspectFrame(shFrame frame, bool full = false);
  static constexpr csv moduleID() { return moduleId; }

public:
  // order dependent
  uint8_t version = 0x00;
  bool padding = false;
  bool extension = false;
  uint8_t ssrc_count = 0;
  uint32_t seq_num = 0;
  uint32_t timestamp = 0;
  uint32_t ssrc = 0;

  ullong_t decipher_len = 0;
  shCipherBuff _m;

  int samples_per_channel = 0;
  int channels = 0;

  Nanos cached_nettime = Nanos::min();
  Nanos cached_time_diff = Nanos::min();
  Nanos sync_wait = Nanos::zero();
  fra::State _state = fra::EMPTY;
  shPeaks peaks_left;
  shPeaks peaks_right;

private:
  // order independent

  uint8v _nonce;
  uint8v _tag;
  uint8v _aad;
  uint8v _payload;

  bool _silence = true;

  static constexpr csv moduleId{"FRAME"};
};

} // namespace pierre