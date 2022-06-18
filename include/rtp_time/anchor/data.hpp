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

#include "core/typedefs.hpp"
#include "rtp_time/rtp_time.hpp"

namespace pierre {
namespace anchor {

using namespace std::chrono_literals;

struct Data {
  uint64_t rate = 0;
  rtp_time::ClockID clockID = 0; // aka clock id
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

  Data &calcNetTime();
  Nanos frameLocalTime(uint32_t frame) const;
  Seconds netTimeElapsed() const;
  Nanos netTimeNow() const;
  bool ok() const { return clockID != 0; }
  bool playing() const { return rate & 0x01; }
  Data &setAt();
  Data &setLocalTimeAt(Nanos local_at = rtp_time::nowNanos());
  Data &setValid(bool set_valid = true);
  Nanos sinceUpdate(const Nanos now = rtp_time::nowNanos()) const { return now - localAtNanos; }

  auto validFor() const { return rtp_time::nowNanos() - at_nanos; }

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
  void dump(csrc_loc loc = src_loc::current()) const;
};

enum Entry : size_t { ACTUAL = 0, LAST, RECENT };

constexpr auto VALID_MIN_DURATION = 5s;
constexpr auto INVALID_DATA = Data();

} // namespace anchor
} // namespace pierre
