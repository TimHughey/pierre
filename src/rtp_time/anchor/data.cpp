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

#include "rtp_time/anchor/data.hpp"
#include "base/input_info.hpp"
#include "base/typical.hpp"
#include "rtp_time/clock.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fmt/format.h>
#include <memory>
#include <optional>

namespace pierre {
using namespace std::chrono_literals;

namespace anchor {
/*
 Using PTP, here is what is necessary
   * The local (monotonic system up)time in nanos (arbitrary reference)
   * The remote (monotonic system up)time in nanos (arbitrary reference)
   * (symmetric) link delay
     1. calculate link delay (PTP)
     2. get local time (PTP)
     3. calculate remote time (nanos) wrt local time (nanos) w/PTP. Now
        we know how remote timestamps align to local ones. Now these
        network times are meaningful.
     4. determine how many nanos elapsed since anchorTime msg egress.
        Note: remote monotonic nanos for iPhones stops when they sleep, though
        not when casting media.
 */

// misc debug
void Data::dump() const {
  const auto hex_fmt_str = FMT_STRING("{:>35}={:#x}\n");
  const auto dec_fmt_str = FMT_STRING("{:>35}={}\n");

  string msg;
  auto w = std::back_inserter(msg);

  fmt::format_to(w, hex_fmt_str, "rate", rate);
  fmt::format_to(w, hex_fmt_str, "clock_id", clock_id);
  fmt::format_to(w, dec_fmt_str, "secs", secs);
  fmt::format_to(w, dec_fmt_str, "frac", frac);
  fmt::format_to(w, hex_fmt_str, "flags", flags);
  fmt::format_to(w, dec_fmt_str, "rtp_time", rtp_time);
  fmt::format_to(w, dec_fmt_str, "network_time", network_time);
  fmt::format_to(w, dec_fmt_str, "valid", valid);
  fmt::format_to(w, dec_fmt_str, "valid_at", valid_at);

  __LOG0(LCOL01 "\n{}\n", moduleID(), csv("DUMP"), msg);
}

} // namespace anchor
} // namespace pierre
