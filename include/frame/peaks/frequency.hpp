// Pierre - Custom Light Show for Wiss Landing
// Copyright (C) 2022  Tim Hughey
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

#include "base/types.hpp"

#include <array>
#include <compare>
#include <fmt/format.h>

namespace pierre {

class Frequency {
public:
  constexpr Frequency() = default;
  constexpr Frequency(auto &&v) noexcept : val(std::move(v)) {}

  explicit constexpr operator double() const noexcept { return val; }

  template <typename T>
    requires std::convertible_to<T, double>
  void assign(T &&v) noexcept {
    std::move(v);
  }

  Frequency &operator=(double &v) noexcept {
    val = v;
    return *this;
  }

  explicit constexpr operator bool() const noexcept { return val != 0.0; }
  constexpr bool operator!() const noexcept { return val == 0.0; }

  constexpr std::partial_ordering operator<=>(auto rhs) const noexcept {
    if (val < rhs) return std::partial_ordering::less;
    if (val > rhs) return std::partial_ordering::greater;

    return std::partial_ordering::equivalent;
  }

  template <class T = Frequency> constexpr T scaled() const noexcept {
    return (val > 0) ? T(std::log10(val)) : T{0};
  }

  /// @brief Support automatic metrics recording via Stats
  /// @return double
  double stat() const noexcept { return val; }

  /// @brief Support automatic metrics recording via Stats
  /// @return Returns the tag to apply to the metric
  static constexpr auto tag() noexcept { return std::array{"comp", "freq"}; }

private:
  double val{0};
};

constexpr Frequency operator""_FREQ(long double freq) { return Frequency{freq}; }

} // namespace pierre

template <> struct fmt::formatter<pierre::Frequency> : formatter<double> {
  // parse is inherited from formatter<double>.
  template <typename FormatContext>
  auto format(const pierre::Frequency &val, FormatContext &ctx) const {
    return formatter<double>::format(static_cast<double>(val), ctx);
  }
};
