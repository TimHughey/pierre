// Pierre
// Copyright (C) 2022 Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#pragma once

#include "base/frequency.hpp"
#include "base/magnitude.hpp"
#include "base/minmax.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <type_traits>

namespace pierre {

using freq_min_max = min_max<Frequency>;
using mag_min_max = min_max<Magnitude>;

/*
class mag_min_max {
public:
  mag_min_max() noexcept { set(0, 100); }
  mag_min_max(const Magnitude a, const Magnitude b) noexcept {

    mag_set.emplace(a);
    mag_set.emplace(b);
  }

  Magnitude interpolate(const mag_min_max &b, const double val) const noexcept {
    //
https://stackoverflow.com/questions/929103/convert-a-number-range-to-another-range-maintaining-ratio
    // OldRange = (OldMax - OldMin)
    // NewRange = (NewMax - NewMin)
    // NewValue = (((OldValue - OldMin) * NewRange) / OldRange) + NewMin

    const Magnitude range_a = max() - min();
    const Magnitude range_b = b.max() - b.min();

    return (((val - min()) * range_b) / range_a) + b.min();
  }

  const Magnitude &max() const noexcept { return *mag_set.rbegin(); }
  const Magnitude &min() const noexcept { return *mag_set.begin(); }

  mag_min_max scaled() const noexcept { return mag_min_max(min().scaled(), max().scaled()); }

  mag_min_max &set(const Magnitude a, const Magnitude b) noexcept {
    *this = mag_min_max(a, b);
    return *this;
  }

private:
  std::set<Magnitude> mag_set;
};

*/
} // namespace pierre
