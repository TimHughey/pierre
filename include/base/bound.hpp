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

#include "base/conf/toml.hpp"
#include "base/types.hpp"

#include <array>
#include <concepts>
#include <fmt/format.h>
#include <tuple>

namespace pierre {

template <typename T, typename U>
concept HasAssignOperator = requires(T v, U t) { v.assign(t); };

template <typename T> struct bound {

  friend fmt::formatter<bound<T>>;

  using storage = std::array<T, 2>;

  constexpr bound() = default;
  constexpr bound(storage &&iv) noexcept : vals{std::move(iv)} {}

  void assign(const toml::array &arr) noexcept {
    if (arr.size() != 2) return;

    auto idx = 0;

    // bound can be specialized for various types from user objects to
    // foundation types.  user object must provide an assign() member
    // function that accepts a toml::table
    //
    // foundation types are directly assigned

    if constexpr (HasAssignOperator<T, toml::table>) {
      arr.for_each([&, this](const toml::table &t) {
        vals[idx].assign(t);

        ++idx;
      });
    } else if constexpr (std::constructible_from<T, double>) {
      arr.for_each([&, this](const toml::value<double> &v) {
        vals[idx] = T(v.get());
        ++idx;
      });
    }
  }

  constexpr const T &first() const { return vals.front(); }

  struct base_pair {
    T min;
    T max;
  };

  constexpr auto get() const noexcept { return base_pair{vals.front(), vals.back()}; }

  constexpr bool inclusive(const bound &v) const noexcept {
    return (v >= vals.front()) && (v <= vals.back());
  }

  constexpr auto max() const noexcept { return vals.back(); }
  constexpr auto min() const noexcept { return vals.front(); }

  constexpr auto scaling() const noexcept { return std::pair{first(), second()}; }

  constexpr const T &second() const { return vals.back(); }

protected:
  storage vals{};
};

} // namespace pierre
