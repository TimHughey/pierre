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

#include <cstdint>
#include <fmt/format.h>
#include <memory>
#include <source_location>

#include "rtp/anchor_info.hpp"

namespace pierre {
namespace rtp {

// this is overkill for this simple of a class
// goal is to have at least well-formed user object copy/assignment example

// construct with AnchorData (copy)
AnchorInfo::AnchorInfo(const AnchorData &ad) : data(ad) { calcNetTime(); }

// construct with AnchorData (move)
AnchorInfo::AnchorInfo(const AnchorData &&ad) : data(std::move(ad)) { calcNetTime(); }

// construct from AnchorInfo (copy)
AnchorInfo::AnchorInfo(const AnchorInfo &ai) : data(ai.data), actual(ai.actual) { calcNetTime(); }

// construct from AnchorInfo (move)
AnchorInfo::AnchorInfo(AnchorInfo &&ai) : data(std::move(ai.data)), actual(std::move(ai.actual)) {
  calcNetTime();
}

// assignment (copy)
AnchorInfo &AnchorInfo::operator=(const AnchorData &ad) {
  data = ad;

  calcNetTime();

  return *this;
}

// assignment (move)
AnchorInfo &AnchorInfo::operator=(AnchorData &&ad) {
  data = std::move(ad);

  calcNetTime();

  return *this;
}

// assignment (copy)
AnchorInfo &AnchorInfo::operator=(const AnchorInfo &ai) {
  data = ai.data;
  actual = ai.actual;

  calcNetTime();

  return *this;
}

// assignment (move)
AnchorInfo &AnchorInfo::operator=(AnchorInfo &&ai) {
  data = std::move(ai.data);
  actual = std::move(ai.actual);

  calcNetTime();

  return *this;
}

void AnchorInfo::calcNetTime() {
  constexpr uint64_t factor_ns = 1000000000;

  // it looks like the networkTimeFrac is a fraction where the msb is work
  // 1/2, the next 1/4 and so on now, convert the network time and fraction
  // into nanoseconds
  uint64_t frac = data.frac;

  frac >>= 32; // reduce precision to about 1/4 nanosecond
  frac *= factor_ns;
  frac >>= 32; // we should now be left with the ns

  frac *= factor_ns; // turn the whole seconds into ns
  data.netTime = data.secs + frac;
}

void AnchorInfo::dump(const std::source_location loc) {
  const auto prefix = loc.function_name();

  fmt::print("{}\n", prefix);
  fmt::print("\t{:>15}={:#x}\n", "rate", data.rate);
  fmt::print("\t{:>15}={:#x}\n", "timelineID", data.timelineID);
  fmt::print("\t{:>15}={:#x}\n", "secs", data.secs);
  fmt::print("\t{:>15}={:#x}\n", "frac", data.frac);
  fmt::print("\t{:>15}={:#x}\n", "flags", data.flags);
  fmt::print("\t{:>15}={:#x}\n", "rtpTime", data.rtpTime);
  fmt::print("\t{:>15}={:#x}\n", "netTime", data.netTime);
}

} // namespace rtp
} // namespace pierre