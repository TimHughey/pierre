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

#include "anchor/net_time.hpp"

#include <cmath>
#include <limits>

namespace pierre {
namespace airplay {

using namespace std::chrono;

NetTime::NetTime(uint64_t secs, uint64_t nano_fracs) {
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

  constexpr int64_t ns_factor = std::pow(10, 9);
  // constexpr double frac_factor = std::pow(2.0, -64.0);
  // constexpr uint64_t frac_mask = std::numeric_limits<uint64_t>::max();

  int64_t net_time_fracs;
  net_time_fracs = nano_fracs >> 32;
  net_time_fracs = net_time_fracs * ns_factor;
  net_time_fracs = net_time_fracs >> 32;

  nanos = Nanos(secs * ns_factor + net_time_fracs);

  // nanos = Secs(secs);
  // nanos += Secs((nano_fracs & frac_mask) * frac_factor);
  //  nanos += Nanos((nano_fracs & frac_mask) >> 32);
}

} // namespace airplay
} // namespace pierre
