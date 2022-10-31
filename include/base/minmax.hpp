// Pierre - Custom Light Show via DMX for Wiss Landing
// Copyright (C) 2021  Tim Hughey
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

#include <algorithm>
#include <cmath>
#include <memory>
#include <tuple>
#include <type_traits>

namespace pierre {

template <typename T> class min_max_pair {
public:
  min_max_pair() noexcept { set(0, 100); }
  min_max_pair(const T a, const T b) noexcept { pair = std::minmax<T>(a, b); }

  static min_max_pair<T> defaults() noexcept { return min_max_pair<T>(0, 100); }

  const T interpolate(const min_max_pair<T> &bpair, const T val) const noexcept {
    // https://stackoverflow.com/questions/929103/convert-a-number-range-to-another-range-maintaining-ratio
    // OldRange = (OldMax - OldMin)
    // NewRange = (NewMax - NewMin)
    // NewValue = (((OldValue - OldMin) * NewRange) / OldRange) + NewMin

    const T range_a = max() - min();
    const T range_b = bpair.max() - bpair.min();

    return (((val - min()) * range_b) / range_a) + bpair.min();
  }

  const T &max() const noexcept { return pair.second; }
  const T &min() const noexcept { return pair.first; }

  auto scaled() const noexcept {
    return min_max_pair<T>(pair.first.scaled(), pair.second.scaled());
  }

  min_max_pair &set(const T a, const T b) noexcept {
    *this = min_max_pair(a, b);
    return *this;
  }

private:
  std::pair<T, T> pair;
};

using min_max_float = min_max_pair<float>;
using min_max_dbl = min_max_pair<double>;

} // namespace pierre
