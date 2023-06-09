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

#include <algorithm>
#include <array>
#include <concepts>
#include <fmt/format.h>
#include <functional>
#include <ranges>

namespace pierre {

template <typename T> struct bound {
  friend fmt::formatter<bound<T>>;

  enum bound_val_t : uint8_t { Low = 0, High };

  constexpr bound() noexcept : vals{} {}
  constexpr bound(std::array<T, 2> &&iv) noexcept : vals{std::move(iv)} {}
  bound(const auto &node) noexcept {}

  template <typename U>
    requires std::convertible_to<T, U>
  bool greater(const U &v) const noexcept {
    return std::greater(v, vals[0]) && std::greater(v, vals[1]);
  }

  template <typename U>
    requires std::convertible_to<T, U>
  bool less(const U &v) const noexcept {
    return std::less(v, vals[0]) && std::less(v, vals[1]);
  }

  template <typename U> bool inclusive(const U &v) const noexcept {
    return std::greater_equal(v, vals[0]) && std::less_equal(v, vals[1]);
  }

  T &operator[](bound_val_t idx) noexcept { return vals[idx]; }

protected:
  std::array<T, 2> vals;
};

} // namespace pierre
