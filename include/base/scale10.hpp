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

#include <array>
#include <cmath>
#include <concepts>
#include <cstdint>

template <typename T>
concept HasDoubleMinMaxPair = requires(T v) {
  { v.get().min.get() } -> std::same_as<double>;
  { v.get().max.get() } -> std::same_as<double>;
};

template <typename T>
concept CanGetDouble = requires(T v) {
  { v.get() } -> std::same_as<double>;
};

namespace pierre {

/// @brief Logarithmically scale a value
/// @tparam T type of value to scale
/// @param val Value to scale
/// @return Scaled value
template <typename T> constexpr T scale10(T val) {
  return (val <= 0.0) ? 0.0 : 10.0 * std::log10(val);
}

template <typename T>
  requires CanGetDouble<T>
constexpr double scale(std::array<double, 4> &&sv, const T &val) noexcept {
  // https://tinyurl.com/tlhscale

  enum scale_val : uint8_t { OldMin = 0, OldMax, NewMin, NewMax };

  auto old_range = sv[OldMax] - sv[OldMin];
  auto new_range = sv[NewMax] - sv[NewMin];

  return (((val.get() - sv[OldMin]) * new_range) / old_range) + sv[NewMin];
}

template <typename T, typename U, typename V>
  requires HasDoubleMinMaxPair<T> && HasDoubleMinMaxPair<U> && CanGetDouble<V>
double scale(const T &t, const U &u, V v) noexcept {
  auto t_pair = t.get();
  auto u_pair = u.get();

  return scale({t_pair.min.get(), t_pair.max.get(), u_pair.min.get(), u_pair.max.get()}, v);
}

} // namespace pierre
