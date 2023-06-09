// /* Pierre - Custom Light Show for Wiss Landing
//     Copyright (C) 2022  Tim Hughey
//
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
//
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the GNU General Public License
//     along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//     https://www.wisslanding.co */

#pragma once

#include "base/types.hpp"

#include <array>
#include <compare>
#include <fmt/format.h>

namespace pierre {

class Magnitude {
public:
  constexpr Magnitude() = default;
  constexpr Magnitude(auto v) noexcept : val(v) {}

  // constexpr operator auto() const noexcept { return val; }
  explicit constexpr operator double() const noexcept { return val; }

  template <typename T>
    requires std::convertible_to<T, double>
  void assign(T &&v) noexcept {
    std::move(v);
  }

  // constexpr double as_dB() const noexcept {
  //     auto dB_at_index(size_t i) const noexcept {

  //   double db_val = 20 * std::log10(reals[i]);

  //   // calculate normalized
  //   // dbNormalized = db - 20 * log10(fftLength * pow(2,N)/2)
  //   return db_val - 20 * std::log10(peaks_max * (power / 2));
  // }
  // }

  explicit constexpr operator bool() const noexcept { return val != 0.0; }
  constexpr bool operator!() const noexcept { return val == 0.0; }

  constexpr std::partial_ordering operator<=>(auto rhs) const noexcept {
    if (val < rhs) return std::partial_ordering::less;
    if (val > rhs) return std::partial_ordering::greater;

    return std::partial_ordering::equivalent;
  }

  template <class T = Magnitude> constexpr T scaled() const noexcept {
    return (val > 0) ? T(10.0 * std::log10(val)) : T{0};
  }

  /// @brief Support automatic metrics recording via Stats
  /// @return double
  double stat() const noexcept { return val; }

  /// @brief Support automatic metrics recording via Stats
  /// @return Returns the tag to apply to the metric
  static constexpr auto tag() noexcept { return std::array{"comp", "mag"}; }

private:
  double val{0};
};

constexpr Magnitude operator""_MAG(long double mag) { return Magnitude{mag}; }

} // namespace pierre

template <> struct fmt::formatter<pierre::Magnitude> : formatter<double> {
  // parse is inherited from formatter<double>.
  template <typename FormatContext>
  auto format(const pierre::Magnitude &val, FormatContext &ctx) const {
    return formatter<double>::format(static_cast<double>(val), ctx);
  }
};
