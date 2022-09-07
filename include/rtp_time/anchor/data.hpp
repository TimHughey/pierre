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

#include "base/input_info.hpp"
#include "base/pet.hpp"
#include "base/typical.hpp"

#include <any>

namespace pierre {
namespace anchor {

struct Data {
  uint64_t rate{0};
  ClockID clock_id{0}; // aka clock id
  uint64_t secs{0};
  uint64_t frac{0};
  uint64_t flags{0};
  uint64_t rtp_time{0};
  Nanos network_time{0};
  Nanos local_time{0};
  Nanos local_at{0};
  bool valid = false;
  Nanos valid_at{0};

  Data &calcNetTime() {
    uint64_t net_time_fracs = 0;

    net_time_fracs >>= 32;
    net_time_fracs *= pet::NS_FACTOR.count();
    net_time_fracs >>= 32;

    network_time = Seconds(secs) + Nanos(frac >> 32);

    // network_time = Nanos(secs * pet::NS_FACTOR.count() + net_time_fracs);

    __LOG0(LCOL01 " network_time={:02.2}\n", module_id, "DEBUG", pet::as_millis_fp(network_time));

    return *this;
  }

  // for frame diff calcs using an alternate time reference
  //   1. returns negative for frame in the past
  //   2. returns positive for frame in future
  //   3. returns nanos::min() when data is not ready
  Nanos frame_diff(uint32_t timestamp) const {
    return ok() ? frame_time(timestamp) - netTimeNow() : Nanos::min();
  }

  Nanos frame_time(uint32_t timestamp) const {
    Nanos calced{0};

    if (valid) {
      int32_t diff_frame = timestamp - rtp_time;
      int64_t diff_ts = (diff_frame * pet::NS_FACTOR.count()) / InputInfo::rate;

      calced = local_time + Nanos(diff_ts);
    }

    return calced;
  }

  uint32_t localTimeFrame(const Nanos time) { // untested
    uint32_t frame_time{0};

    if (valid) {
      Nanos diff_time = time - local_time;
      int64_t diff_frame = (diff_time.count() * InputInfo::rate) / pet::NS_FACTOR.count();
      int32_t diff_frame32 = diff_frame;
      frame_time = rtp_time + diff_frame32;
    }

    return frame_time;
  }

  SecondsFP netTimeElapsed() const { return pet::elapsed_as<SecondsFP>(netTimeNow() - valid_at); }

  Nanos netTimeNow() const { return valid_at + pet::elapsed_abs_ns(valid_at); }

  bool ok() const { return clock_id != 0; }
  csv render_mode() const { return rendering() ? RENDERING : NOT_RENDERING; }
  bool rendering() const { return rate & 0x01; }

  Data &setLocalTimeAt(Nanos _local_at = pet::now_nanos()) {
    local_at = _local_at;
    return *this;
  }

  Data &setValid(bool set_valid = true) {
    valid = set_valid;
    valid_at = pet::now_nanos();
    return *this;
  }

  Nanos sinceUpdate(const Nanos now = pet::now_nanos()) const { return now - valid_at; }

  auto validFor() const { return pet::now_nanos() - valid_at; }

  static Data any_cast(std::any &data) {
    return data.has_value() ? std::any_cast<Data>(data) : Data();
  }

  // return values:
  // -1 = clock is different
  //  0 = clock, rtp_time, network_time are equal
  // +1 = clock is same, rtp_time and network_time are different
  friend int operator<=>(const Data &lhs, const Data &rhs) {
    if (lhs.clock_id != rhs.clock_id) {
      return -1;
    }

    if ((lhs.clock_id == rhs.clock_id) &&       // clock ID same
        (lhs.rtp_time == rhs.rtp_time) &&       // rtpTime same
        (lhs.network_time == rhs.network_time)) // network_time same
    {
      return 0;
    }

    return +1;
  }

  // misc debug
  static constexpr csv moduleID() { return module_id; }
  void dump() const;

private:
  static constexpr csv RENDERING{"rendering"};
  static constexpr csv NOT_RENDERING{"not rendering"};
  static constexpr csv module_id{"ANCHOR_DATA"};
};

enum Entry : size_t { ACTUAL = 0, LAST, RECENT };

constexpr auto VALID_MIN_DURATION = 5s;
constexpr auto INVALID_DATA = Data();

} // namespace anchor
} // namespace pierre
