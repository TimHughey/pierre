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

#include "base/minmax.hpp"

#include <algorithm>
#include <initializer_list>
#include <tuple>
#include <type_traits>
#include <vector>

namespace pierre {

template <typename T> class hard_soft {
public:
  constexpr hard_soft() noexcept : vals{0, 0, 100, 100} {}
  constexpr hard_soft(std::initializer_list<T> initial_vals) : vals(initial_vals) { sort(); }
  constexpr hard_soft(const T a, const T b, const T c, const T d) noexcept : vals{a, b, c, d} {
    sort();
  }

  constexpr min_max<T> hard() const noexcept { return min_max<T>(vals[0], vals[3]); }

  constexpr bool inclusive(const T v) const noexcept { return (v >= vals[0]) && (v <= vals[3]); }
  constexpr bool inclusive_hard(const T v) const noexcept { return inclusive(v); }
  constexpr bool inclusive_soft(const T v) const noexcept {
    return (v >= vals[1]) && (v <= vals[2]);
  }

  constexpr T interplolate(const T v) const noexcept { return hard().interpolate(v); }
  constexpr T interploate_hard(const T v) const noexcept { return interploate(v); }
  constexpr T interploate_soft(const T v) const noexcept { return soft().interpolate(v); }

  constexpr auto scaled() const noexcept { return hard().scaled(); }
  constexpr auto scaled_hard() const noexcept { return scaled(); }
  constexpr auto scaled_soft() const noexcept { return soft().scaled(); }

  constexpr min_max<T> soft() const noexcept { return min_max<T>(vals[1], vals[2]); }

private:
  void sort() noexcept { std::sort(vals.begin(), vals.end()); }

private:
  std::vector<T> vals;
};

} // namespace pierre
