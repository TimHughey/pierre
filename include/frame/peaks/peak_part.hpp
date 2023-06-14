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

struct _freq_tag {};
struct _dB_tag {};

template <typename T>
concept IsPeakPartFrequency = std::same_as<T, peak_part<_freq_tag>>;

template <typename T>
concept isPeakPartdB = std::same_as<T, peak_part<_dB_tag>>;

template <typename T>
concept IsSpecializedPeakPart = IsPeakPartFrequency<T> || isPeakPartdB<T>;

template <typename TAG> struct peak_part {

  friend fmt::formatter<peak_part>;

  constexpr peak_part() noexcept {
    if constexpr (std::same_as<TAG, _dB_tag>) {
      ppv = -96.0;
    } else {
      ppv = 0.0;
    }
  }
  constexpr peak_part(const peak_part &) = default;
  constexpr peak_part(double v) noexcept { assign(v); }

  constexpr peak_part &operator=(const peak_part &) = default;
  constexpr peak_part &operator=(peak_part &&) = default;

  void assign(const peak_part &pp) noexcept { ppv = pp.ppv; }
  void assign(double v) noexcept { ppv = v; }

  void clear() noexcept { ppv = 0.0; }

  constexpr explicit operator bool() const noexcept { return ppv; }
  constexpr explicit operator double() const noexcept { return ppv; }

  template <typename U> auto lerp(std::pair<U, U> &&a_b) const noexcept {
    return lerp(a_b.first, a_b.second);
  }

  template <typename U> auto lerp(U a, U b) const noexcept {
    double val{0};

    if constexpr (std::same_as<TAG, _freq_tag>) {
      // convert freq and magnitude into a linear number space
      val = std::log10(ppv);
    } else if constexpr (std::same_as<TAG, _dB_tag>) {
      val = ppv;
    }

    return U{std::lerp(static_cast<double>(a), static_cast<double>(b), val)};
  }

  constexpr peak_part linear() const noexcept {
    if constexpr (std::same_as<TAG, _freq_tag>) {
      return std::log10(ppv);
    } else {
      // https://tinyurl.com/tlhdblinear
      return std::pow(10.0, ppv / 10.0);
    }
  }

  /// @brief Enable all comparison operators for peak parts
  /// @param  peak_part right hand side
  /// @return same as double <=>
  constexpr auto operator<=>(const peak_part &) const = default;

  peak_part &operator*=(const peak_part &rhs) {
    ppv *= rhs.ppv;

    return *this;
  }

  // friends defined inside class body are inline and are hidden from non-ADL lookup
  friend peak_part
  operator*(peak_part lhs,        // passing lhs by value helps optimize chained a+b+c
            const peak_part &rhs) // otherwise, both parameters may be const references
  {
    lhs *= rhs; // reuse compound assignment
    return lhs; // return the result by value (uses move constructor)
  }

  peak_part &operator-=(const peak_part &rhs) {
    ppv -= rhs.ppv;

    return *this;
  }

  // friends defined inside class body are inline and are hidden from non-ADL lookup
  friend peak_part
  operator-(peak_part lhs,        // passing lhs by value helps optimize chained a+b+c
            const peak_part &rhs) // otherwise, both parameters may be const references
  {
    lhs -= rhs; // reuse compound assignment
    return lhs; // return the result by value (uses move constructor)
  }

  peak_part &operator+=(const peak_part &rhs) {
    ppv += rhs.ppv;

    return *this;
  }

  // friends defined inside class body are inline and are hidden from non-ADL lookup
  friend peak_part
  operator+(peak_part lhs,        // passing lhs by value helps optimize chained a+b+c
            const peak_part &rhs) // otherwise, both parameters may be const references
  {
    lhs += rhs; // reuse compound assignment
    return lhs; // return the result by value (uses move constructor)
  }

  peak_part &operator/=(const peak_part &rhs) {
    ppv /= rhs.ppv;
    return *this;
  }

  // friends defined inside class body are inline and are hidden from non-ADL lookup
  friend peak_part
  operator/(peak_part lhs,        // passing lhs by value helps optimize chained a+b+c
            const peak_part &rhs) // otherwise, both parameters may be const references
  {
    lhs /= rhs; // reuse compound assignment
    return lhs; // return the result by value (uses move constructor)
  }

  // constexpr peak_part &scaled() noexcept {
  //   if (ppv <= 0.0) {
  //     ppv = 0.0;
  //   } else {
  //     if constexpr (std::same_as<TAG, _freq_tag>) {
  //       ppv = std::log10(ppv);
  //     } else if constexpr (std::same_as<TAG, _dB>) {
  //       ppv = 10.0 * std::log10(ppv);
  //     }
  //   }

  //   return *this;
  // }

  auto stat() const noexcept { return ppv; }

  constexpr auto tag() const noexcept {
    if constexpr (std::same_as<TAG, _freq_tag>) {
      return std::array{"comp", "freq"};
    } else {
      return std::array{"comp", "dB"};
    }
  }

private:
  double ppv;
};

using Freq = peak_part<_freq_tag>;
using dB = peak_part<_dB_tag>;

} // namespace pierre

template <typename T>
  requires pierre::IsSpecializedPeakPart<T>
struct fmt::formatter<T> : formatter<double> {
  // parse is inherited from formatter<double>.
  template <typename FormatContext> auto format(const T &val, FormatContext &ctx) const {
    return formatter<double>::format(static_cast<double>(val), ctx);
  }
};