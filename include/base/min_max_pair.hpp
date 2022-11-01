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
#include <concepts>
#include <memory>
#include <tuple>
#include <type_traits>

namespace pierre {

template <typename T> class min_max_pair {
public:
  constexpr min_max_pair() noexcept : pair(T(0), T(100)) {}
  constexpr min_max_pair(const T a, const T b) noexcept : pair(std::minmax<T>(a, b)) {}

  constexpr bool inclusive(T val) const noexcept {
    return (val >= pair.first) && (val <= pair.second);
  }

  constexpr T interpolate(const auto &bpair, const T val) const noexcept {
    // https://stackoverflow.com/questions/929103/convert-a-number-range-to-another-range-maintaining-ratio
    // OldRange = (OldMax - OldMin)
    // NewRange = (NewMax - NewMin)
    // NewValue = (((OldValue - OldMin) * NewRange) / OldRange) + NewMin

    const T range_a = max() - min();
    const T range_b = bpair.max() - bpair.min();

    return (((val - min()) * range_b) / range_a) + bpair.min();
  }

  constexpr const T &max() const noexcept { return pair.second; }
  constexpr const T &min() const noexcept { return pair.first; }

  constexpr auto scaled() const noexcept {
    return min_max_pair<T>(pair.first.scaled(), pair.second.scaled());
  }

private:
  const std::pair<T, T> pair;
};

// class mag_min_max {
// public:
//   mag_min_max() noexcept { set(0, 100); }
//   mag_min_max(const Magnitude a, const Magnitude b) noexcept {
//
//     mag_set.emplace(a);
//     mag_set.emplace(b);
//   }
//
//   Magnitude interpolate(const mag_min_max &b, const double val) const noexcept {
//     //
// https://stackoverflow.com/questions/929103/convert-a-number-range-to-another-range-maintaining-ratio
//     // OldRange = (OldMax - OldMin)
//     // NewRange = (NewMax - NewMin)
//     // NewValue = (((OldValue - OldMin) * NewRange) / OldRange) + NewMin
//
//     const Magnitude range_a = max() - min();
//     const Magnitude range_b = b.max() - b.min();
//
//     return (((val - min()) * range_b) / range_a) + b.min();
//   }
//
//   const Magnitude &max() const noexcept { return *mag_set.rbegin(); }
//   const Magnitude &min() const noexcept { return *mag_set.begin(); }
//
//   mag_min_max scaled() const noexcept { return mag_min_max(min().scaled(), max().scaled()); }
//
//   mag_min_max &set(const Magnitude a, const Magnitude b) noexcept {
//     *this = mag_min_max(a, b);
//     return *this;
//   }
//
// private:
//   std::set<Magnitude> mag_set;
// };

} // namespace pierre
