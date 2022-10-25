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

namespace pierre {

class Frequency {
public:
  Frequency() = default;
  Frequency(auto v) noexcept : val(v) {}

  operator auto() const noexcept { return val; }

  std::partial_ordering operator<=>(auto rhs) const noexcept {
    if (val < rhs) return std::partial_ordering::less;
    if (val > rhs) return std::partial_ordering::greater;

    return std::partial_ordering::equivalent;
  }

private:
  double val{0};
};

} // namespace pierre

template <> struct fmt::formatter<pierre::Frequency> : formatter<double> {
  // parse is inherited from formatter<double>.
  template <typename FormatContext>
  auto format(const pierre::Frequency &val, FormatContext &ctx) const {
    return formatter<double>::format(val, ctx);
  }
};
