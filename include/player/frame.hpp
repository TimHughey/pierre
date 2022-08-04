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
#include "base/pe_time.hpp"
#include "base/typical.hpp"
#include "base/uint8v.hpp"
#include "dsp/peak_info.hpp"
#include "dsp/peaks.hpp"
#include "player/flush_request.hpp"
#include "player/frame_time.hpp"

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
constexpr csv LATE{"late"};
constexpr csv NONE{"none"};
constexpr csv NOT_READY{"not_ready"};
constexpr csv OUTDATED{"outdated"};
constexpr csv PLAYABLE{"playable"};
constexpr csv PLAYED{"played"};

StateKeys &state_keys();
} // namespace fra

namespace player {

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
  static constexpr size_t PAYLOAD_MIN_SIZE = 24;

private:
  Frame(uint8v &packet);
  Frame(uint8_t version, fra::State state, bool silence)
      : version(version), _state(state), _peaks_left(Peaks::create()),
        _peaks_right(Peaks::create()), _silence(silence) {}

public:
  // Object Creation
  static shFrame create(uint8v &packet) { //
    return std::shared_ptr<Frame>(new Frame(packet));
  }
  static shFrame createSilence() { //
    return std::shared_ptr<Frame>(new Frame(0x02, fra::PLAYABLE, true));
  }

  // Public API
  void cleanup() { _m.reset(); }
  bool decipher();                   // sodium decipher packet
  static void decode(shFrame frame); // definition must be in cpp
  void decodeOk() { _state = fra::DECODED; }
  static shFrame ensureFrame(shFrame frame) { return ok(frame) ? frame : createSilence(); }
  static void findPeaks(shFrame frame); // defnition must be in cpp
  bool isReady() const { return _peaks_left.use_count() && _peaks_right.use_count(); }
  bool isValid() const { return version == 0x02; }

  bool keep(FlushRequest &flush);

  // NOTE: localTime() and localTimeDiff() must be defined in .cpp due to intentional
  // limited visibility of Anchor
  const Nanos localTimeDiff(); // calculate diff between local and frame time
  shFrame markPlayed(auto &played, auto &not_played) {
    // note: when called on a nextFrame() it is safe to allow outdated frames
    // because they passed FrameTimeDiff late check

    if (stateEqual({fra::LATE, fra::PLAYABLE})) {
      _state = fra::PLAYED;
      ++played;
    } else {
      ++not_played;
    }

    return shared_from_this();
  }

  // nextFrame() returns true when searching should stop; false to keep searching
  bool nextFrame(const FrameTimeDiff &frame_time_diff, fra::StatsMap &stats_map);

  uint8v &payload() { return _payload; }
  size_t payloadSize() const { return _payload.size(); }
  PeakInfo peakInfo(Elapsed &uptime); // must be in .cpp due to Anchor dependency

  // state
  bool decoded() const { return stateEqual(fra::DECODED); }
  static bool empty(shFrame frame) { return ok(frame) && frame->stateEqual(fra::EMPTY); }
  bool future() const { return stateEqual(fra::FUTURE); }
  static bool ok(shFrame frame) { return frame.use_count(); }
  bool outdated() const { return stateEqual(fra::OUTDATED); }
  bool playable() const { return stateEqual(fra::PLAYABLE); }
  bool played() const { return stateEqual(fra::PLAYED); }
  bool purgeable() const { return stateEqual({fra::OUTDATED, fra::PLAYED}); }
  bool silence() const { return _silence; }
  static fra::StateConst &stateVal(shFrame frame);
  bool stateEqual(fra::StateConst &check) const { return check == _state; }
  bool stateEqual(const fra::States &states) const {
    return ranges::any_of(states, [&](const auto &state) { return stateEqual(state); });
  }
  bool unplayed() const { return stateEqual({fra::DECODED, fra::FUTURE, fra::PLAYABLE}); }

  // stats

  // create an empty StatsMap
  static fra::StatsMap statsMap();

  // add a state to a StatsMap
  void statsAdd(fra::StatsMap &stats_map, fra::State override = fra::NONE) {
    // use override, if specified
    ++stats_map[(override == fra::NONE) ? _state : override];
  }

  // create a textual representation of the StatsMap
  static auto &statsMsg(auto &msg, const fra::StatsMap &map) {
    ranges::for_each(map, [w = std::back_inserter(msg)](auto &key_val) {
      const auto &[key, val] = key_val;

      if (val > 0) {
        fmt::format_to(w, "{}={:<5} ", key, val);
      }
    });

    return msg;
  }

  // class member functions
  friend void swap(Frame &a, Frame &b); // swap specialization

  // misc debug
  const string inspect() { return inspectFrame(shared_from_this()); }
  static const string inspectFrame(shFrame frame, bool full = false);
  static constexpr csv moduleID() { return moduleId; }

public:
  // order independent
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

private:
  // order independent
  fra::State _state = fra::EMPTY;
  uint8v _nonce;
  uint8v _tag;
  uint8v _aad;
  uint8v _payload;
  shPeaks _peaks_left;
  shPeaks _peaks_right;
  bool _silence = true;
  Nanos local_time_diff;
  std::any anchor_data;

  static constexpr csv moduleId{"FRAME"};
};

} // namespace player
} // namespace pierre