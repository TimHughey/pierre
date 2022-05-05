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
#include <source_location>
#include <vector>

namespace pierre {
namespace anchor {
namespace time {

using namespace std::chrono_literals;
using Nanos = std::chrono::nanoseconds;
using SteadyClock = std::chrono::steady_clock;
using Secs = std::chrono::seconds;
using src_loc = std::source_location;

typedef const char *ccs;
typedef const src_loc csrc_loc;

class Net {
public:
  Net() = default;
  Net(uint64_t ticks) : nanos(ticks) {}
  Net(uint64_t secs, uint64_t nano_fracs);

  const Nanos &ns() const { return nanos; }

  uint64_t ticks() const { return nanos.count(); }

  bool tooOld(const auto &duration) const { return (nanos < (nanos - duration)); }

  // misc debug
  void dump(csrc_loc loc = src_loc::current()) const;
  ccs fnName(csrc_loc loc = src_loc::current()) const { return loc.function_name(); }

private:
  Nanos nanos = Nanos::min();
};
} // namespace time
} // namespace anchor
} // namespace pierre