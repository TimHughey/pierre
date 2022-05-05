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

#include "anchor/time/net.hpp"

namespace pierre {
namespace anchor {
namespace time {

Net::Net(uint64_t secs, uint64_t nano_fracs) {
  // it looks like the nano_fracs is a fraction where the msb is work 1/2
  // the next 1/4 and so on now, convert the network time and fraction into nanoseconds
  constexpr uint64_t ns_factor = 1 ^ 9;

  // begin tallying up the nanoseconds starting with the seconds
  nanos = Secs(secs);

  // convert the nano_fracs into actual nanoseconds
  nano_fracs >>= 32; // reduce precision to about 1/4 of a ns
  nano_fracs *= ns_factor;
  nano_fracs >>= 32; // shift again to get ns

  // add the fractional nanoseconds to the tally
  nanos += Nanos(nano_fracs);
}

void Net::dump(csrc_loc loc) const {
  auto constexpr f = FMT_STRING("{} ticks={:#x}\n");
  fmt::print(f, fnName(loc), ticks());
}

} // namespace time
} // namespace anchor
} // namespace pierre
