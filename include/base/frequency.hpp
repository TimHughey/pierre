/* Pierre - Custom Light Show for Wiss Landing
    Copyright (C) 2022  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.co */

#pragma once

#include "base/types.hpp"

#include <compare>
#include <fmt/format.h>
#include <type_traits>

// #include <algorithm>
// #include <cctype>
// #include <cstddef>
// #include <cstdint>
// #include <iterator>
// #include <memory>
// #include <ranges>
// #include <vector>

// namespace {
// namespace ranges = std::ranges;
// } // namespace

namespace pierre {

class Frequency {
public:
  Frequency() = default;
  Frequency(auto v) noexcept : val(v) {}

  operator auto() const noexcept { return val; }

  std::strong_ordering operator<=>(auto rhs) const noexcept {
    if (rhs < val) return std::strong_ordering::less;
    if (rhs > val) return std::strong_ordering::greater;

    return std::strong_ordering::equal;
  }

private:
  float val{0};
};

} // namespace pierre

template <> struct fmt::formatter<pierre::Frequency> : formatter<float> {
  // parse is inherited from formatter<string_view>.
  template <typename FormatContext>
  auto format(const pierre::Frequency &val, FormatContext &ctx) const {
    return formatter<float>::format(val, ctx);
  }
};
