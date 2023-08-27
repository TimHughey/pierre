//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2023  Tim Hughey
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

#include "base/types.hpp"

#include <array>
#include <compare>
#include <fmt/format.h>
#include <tuple>
#include <type_traits>

namespace pierre {

template <typename T> struct peak_part;

namespace peak {
struct freq_tag {};
struct mag_tag {};
struct spl_tag {};

} // namespace peak

template <typename T>
concept IsPeakPartFrequency = std::same_as<T, peak_part<peak::freq_tag>>;

template <typename T>
concept IsPeakPartMag = std::same_as<T, peak_part<peak::mag_tag>>;

template <typename T>
concept IsPeakPartSpl = std::same_as<T, peak_part<peak::spl_tag>>;

template <typename T>
concept IsSpecializedPeakPart =
    IsAnyOf<T, peak_part<peak::freq_tag>, peak_part<peak::mag_tag>, peak_part<peak::spl_tag>>;

template <typename TAG> struct peak_part {

  friend fmt::formatter<peak_part>;

  constexpr peak_part() noexcept : ppv{0.0} {}

  constexpr peak_part(const peak_part &) = default;
  constexpr peak_part(double v) noexcept { assign(v); }

  constexpr peak_part &operator=(const peak_part &) = default;
  constexpr peak_part &operator=(peak_part &&) = default;

  void assign(const peak_part &pp) noexcept { ppv = pp.ppv; }
  void assign(double v) noexcept { ppv = v; }

  void clear() noexcept { ppv = 0.0; }

  /// @brief Get copy of foundational type
  /// @return Foundational type, by value
  constexpr auto get() const noexcept { return ppv; }

  constexpr peak_part linear() const noexcept {
    if constexpr (std::same_as<TAG, peak::freq_tag>) {
      return std::log10(ppv);
    } else {
      // https://tinyurl.com/tlhdblinear
      return std::pow(10.0, ppv / 10.0);
    }
  }

  constexpr explicit operator double() const noexcept { return ppv; }

  operator bool() const = delete;
  bool operator!() const = delete;

  /// @brief Enable all comparison operators for peak parts
  /// @param  peak_part right hand side
  /// @return same as double <=>
  constexpr auto operator<=>(const peak_part &) const = default;

  peak_part &operator*=(const peak_part &rhs) {
    ppv *= rhs.ppv;

    return *this;
  }

  // friends defined inside class body are inline and are hidden from non-ADL lookup
  // passing lhs by value helps optimize chained a+b+c
  friend peak_part operator*(peak_part lhs, const peak_part &rhs) {
    lhs *= rhs; // reuse compound assignment
    return lhs; // return the result by value (uses move constructor)
  }

  peak_part &operator-=(const peak_part &rhs) {
    ppv -= rhs.ppv;

    return *this;
  }

  // friends defined inside class body are inline and are hidden from non-ADL lookup
  // passing lhs by value helps optimize chained a+b+c
  friend peak_part operator-(peak_part lhs, const peak_part &rhs) {
    lhs -= rhs; // reuse compound assignment
    return lhs; // return the result by value (uses move constructor)
  }

  peak_part &operator+=(const peak_part &rhs) {
    ppv += rhs.ppv;

    return *this;
  }

  // friends defined inside class body are inline and are hidden from non-ADL lookup
  // passing lhs by value helps optimize chained a+b+c
  friend peak_part operator+(peak_part lhs, const peak_part &rhs) {
    lhs += rhs; // reuse compound assignment
    return lhs; // return the result by value (uses move constructor)
  }

  peak_part &operator/=(const peak_part &rhs) {
    ppv /= rhs.ppv;
    return *this;
  }

  // friends defined inside class body are inline and are hidden from non-ADL lookup
  // passing lhs by value helps optimize chained a+b+c
  friend peak_part operator/(peak_part lhs, const peak_part &rhs) {
    lhs /= rhs; // reuse compound assignment
    return lhs; // return the result by value (uses move constructor)
  }

  /// @brief Access the underlying foundational type
  /// @return Reference to underlying foundational type
  constexpr auto &raw() noexcept { return ppv; }

  auto stat() const noexcept { return ppv; }

  constexpr auto tag() const noexcept {
    if constexpr (std::same_as<TAG, peak::freq_tag>) {
      return std::array{"comp", "freq"};
    } else if constexpr (std::same_as<TAG, peak::spl_tag>) {
      return std::array{"comp", "spl"};
    } else if constexpr (std::same_as<TAG, peak::mag_tag>) {
      return std::array{"comp", "mag"};
    }
  }

private:
  double ppv;
};

namespace peak {
using Freq = peak_part<freq_tag>;
using Mag = peak_part<mag_tag>;
using Spl = peak_part<spl_tag>;
} // namespace peak
} // namespace pierre

template <typename T>
  requires pierre::IsSpecializedPeakPart<T>
struct fmt::formatter<T> : formatter<double> {
  // parse is inherited from formatter<double>.
  template <typename FormatContext> auto format(const T &val, FormatContext &ctx) const {
    return formatter<double>::format(static_cast<double>(val), ctx);
  }
};