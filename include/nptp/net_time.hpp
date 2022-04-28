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

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fmt/format.h>
#include <iterator>
#include <pthread.h>
#include <source_location>
#include <vector>

namespace pierre {
namespace nptp {

using namespace std::chrono_literals;

class NetTime {
public:
  using nanoseconds = std::chrono::nanoseconds;
  using Clock = std::chrono::steady_clock;
  using Nanos = std::chrono::duration<int64_t, std::nano>;
  using Moment = std::chrono::time_point<Clock, Nanos>;
  using Seconds = std::chrono::seconds;

public:
  NetTime(){};

  NetTime(uint64_t ticks) { nanos = nanoseconds(ticks); }

  NetTime(uint64_t seconds, uint64_t nano_fracs) {
    // it looks like the nano_fracs is a fraction where the msb is work 1/2
    // the next 1/4 and so on now, convert the network time and fraction into nanoseconds
    constexpr uint64_t ns_factor = 1 ^ 9;

    // begin tallying up the nanoseconds starting with the seconds
    nanos = std::chrono::duration_cast<nanoseconds>(Seconds(seconds));

    // convert the nano_fracs into actual nanoseconds
    nano_fracs >>= 32; // reduce precision to about 1/4 of a ns
    nano_fracs *= ns_factor;
    nano_fracs >>= 32; // shift again to get ns

    // add the fractional nanoseconds to the tally
    nanos += nanoseconds(nano_fracs);
  }

  const Nanos &ns() const { return nanos; }

  uint64_t ticks() const { return nanos.count(); }

  bool tooOld(const auto &duration) const { return (nanos < (nanos - duration)); }

  void dump(const std::source_location loc = std::source_location::current()) {
    auto fmt_str = FMT_STRING("{} ticks={:#x}\n");
    fmt::print(fmt_str, fnName(loc), ticks());
  }

private:
  static const char *fnName(const std::source_location loc = std::source_location::current()) {
    return loc.function_name();
  }

private:
  Nanos nanos = Nanos::min();
};

} // namespace nptp
} // namespace pierre