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

#include "base/clock_info.hpp"
#include "base/pet.hpp"
#include "base/typical.hpp"

namespace pierre {

class RefClock {

public:
  RefClock(const Nanos ref = Nanos::zero())
      : ref(ref), // reference offset
        local_ref(pet::now_monotonic()) {}

  // NOTE: new info is updated every 126ms
  const ClockInfo info() {

    const Nanos offset = local_ref - pet::now_monotonic();

    return ClockInfo // calculate a local view of the nqptp data
        {.clock_id = 0x01,
         .masterClockIp = MasterIP{0},
         .sampleTime = static_cast<uint64_t>(pet::now_monotonic().count()),
         .rawOffset = static_cast<uint64_t>(pet::now_monotonic().count() - local_ref.count()),
         .mastershipStartTime = ref};
  }

  bool ok() { return info().ok(); };

  // misc debug
  void dump(csv module_id = csv{"REF_CLOCK"}) {
    __LOG0(LCOL01 " inspect info\n{}\n", module_id, csv("DUMP"), info().inspect());
  }

private:
  // order dependent
  const Nanos ref;
  const Nanos local_ref;
};

} // namespace pierre
