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

#include "clock/info.hpp"

#include "fmt/format.h"

namespace pierre {
namespace airplay {
namespace clock {

bool Info::ok(auto age) const {
  const auto old_ns = SteadyClock::duration(age).count();
  return ((int64_t)sampleTime < ((int64_t)now() - (int64_t)old_ns) ? true : false);
}

uint64_t Info::now() { return SteadyClock::now().time_since_epoch().count(); }

void Info::dump() const {
  const auto now_ns = now();

  const int64_t now_minus_sample_time = (int64_t)now_ns - (int64_t)sampleTime;

  const auto hex_fmt_str = FMT_STRING("{:>35}={:#x}\n");
  const auto dec_fmt_str = FMT_STRING("{:>35}={}\n");

  fmt::print("{}\n", fnName());
  fmt::print(hex_fmt_str, "clockId", clockID);
  fmt::print(dec_fmt_str, "now", now_ns);
  fmt::print(dec_fmt_str, "mastershipStart", mastershipStartTime);
  fmt::print(dec_fmt_str, "rawOffset", rawOffset);
  fmt::print(dec_fmt_str, "sampleTime", sampleTime);
  fmt::print(dec_fmt_str, "now - sampleTime", now_minus_sample_time);
  fmt::print("\n");
}

} // namespace clock
} // namespace airplay
} // namespace pierre