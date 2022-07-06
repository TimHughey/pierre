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

#include "base/pe_time.hpp"
#include "base/typical.hpp"
#include "core/input_info.hpp"

namespace pierre {
namespace anchor {

using namespace std::chrono_literals;

struct Data {
  uint64_t rate = 0;
  ClockID clockID = 0; // aka clock id
  uint64_t secs = 0;
  uint64_t frac = 0;
  uint64_t flags = 0;
  uint64_t rtpTime = 0;
  uint64_t networkTime = 0; // from set anchor packet
  Nanos localTime{0};
  Nanos at_nanos{0};
  Nanos localAtNanos{0};
  bool valid = false;
  Nanos valid_at_ns{0};

  Data &calcNetTime() {
    uint64_t net_time_fracs = 0;

    net_time_fracs >>= 32;
    net_time_fracs *= pe_time::NS_FACTOR.count();
    net_time_fracs >>= 32;

    networkTime = secs * pe_time::NS_FACTOR.count() + net_time_fracs;

    return *this;
  }

  Nanos frameLocalTime(uint32_t timestamp) const {
    Nanos local_time{0};

    if (valid) {
      int32_t diff_frame = timestamp - rtpTime;
      int64_t diff_ts = (diff_frame * pe_time::NS_FACTOR.count()) / InputInfo::rate;

      local_time = localTime + Nanos(diff_ts);
    }

    return local_time;
  }

  Seconds netTimeElapsed() const {
    return pe_time::elapsed_as<Seconds>(netTimeNow() - valid_at_ns);
  }

  Nanos netTimeNow() const { return valid_at_ns + pe_time::elapsed_abs_ns(valid_at_ns); }

  bool ok() const { return clockID != 0; }
  bool playing() const { return rate & 0x01; }
  Data &setAt() {
    at_nanos = pe_time::nowNanos();
    return *this;
  }

  Data &setLocalTimeAt(Nanos local_at = pe_time::nowNanos()) {
    localAtNanos = local_at;
    return *this;
  }

  Data &setValid(bool set_valid = true) {
    valid = set_valid;
    valid_at_ns = pe_time::nowNanos();
    return *this;
  }

  Nanos sinceUpdate(const Nanos now = pe_time::nowNanos()) const { return now - localAtNanos; }

  auto validFor() const { return pe_time::nowNanos() - at_nanos; }

  // return values:
  // -1 = clock is different
  //  0 = clock, rtpTime, networkTime are equal
  // +1 = clock is same, rtpTime and networkTime are different
  friend int operator<=>(const Data &lhs, const Data &rhs) {
    if (lhs.clockID != rhs.clockID) {
      return -1;
    }

    if ((lhs.clockID == rhs.clockID) &&       // clock ID same
        (lhs.rtpTime == rhs.rtpTime) &&       // rtpTime same
        (lhs.networkTime == rhs.networkTime)) // networkTime same
    {
      return 0;
    }

    return +1;
  }

  // misc debug
  auto moduleID() const { return module_id; }
  void dump() const;

private:
  static constexpr csv module_id{"ANCHOR_DATA"};
};

enum Entry : size_t { ACTUAL = 0, LAST, RECENT };

constexpr auto VALID_MIN_DURATION = 5s;
constexpr auto INVALID_DATA = Data();

} // namespace anchor
} // namespace pierre
