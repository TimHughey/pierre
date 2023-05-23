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

#include "anchor_last.hpp"
#include "base/dura.hpp"

namespace pierre {

void AnchorLast::update(const AnchorData &ad, const ClockInfo &clock) noexcept {

  rtp_time = ad.rtp_time;
  anchor_time = ad.anchor_time;
  dura::apply_offset(localized, anchor_time, clock.rawOffset);
  // localized = dura::subtract_offset(anchor_time, clock.rawOffset);
  since_update.reset();

  if (clock_id == 0x00) { // only update master when AnchorLast isn't ready
    master_at = clock.mastershipStartTime;
    clock_id = ad.clock_id; // denotes AnchorLast is ready
  }
}

} // namespace pierre
