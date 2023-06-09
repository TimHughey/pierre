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

#include <compare>
#include <fmt/format.h>

namespace pierre {
namespace desk {

template <typename T> struct color_part;

struct _bri_tag {};
struct _hue_tag {};
struct _sat_tag {};

template <typename T>
concept IsColorPart = requires(T p) { requires std::same_as<decltype(p.cpv), double>; };

template <typename T>
concept IsColorPartBriOrSat = IsAnyOf<T, color_part<_bri_tag>, color_part<_sat_tag>>;

template <typename T>
concept IsColorPartHue = std::same_as<T, color_part<_hue_tag>>;

template <typename T>
concept IsSpecializedColorPart = IsColorPartBriOrSat<T> || IsColorPartHue<T>;

template <typename TAG> struct color_part {

  friend fmt::formatter<color_part>;

  constexpr color_part() = default;
  constexpr color_part(double v) noexcept { assign(v); }

  constexpr color_part(const color_part &) = default;
  constexpr color_part(color_part &&) = default;

  constexpr color_part &operator=(const color_part &) = default;
  constexpr color_part &operator=(color_part &&) = default;

  color_part &operator*=(const color_part &rhs) {
    cpv *= rhs.cpv;

    return *this;
  }

  // friends defined inside class body are inline and are hidden from non-ADL lookup
  friend color_part
  operator*(color_part lhs,        // passing lhs by value helps optimize chained a+b+c
            const color_part &rhs) // otherwise, both parameters may be const references
  {
    lhs *= rhs; // reuse compound assignment
    return lhs; // return the result by value (uses move constructor)
  }

  color_part &operator-=(const color_part &rhs) {
    cpv -= rhs.cpv;

    return *this;
  }

  // friends defined inside class body are inline and are hidden from non-ADL lookup
  friend color_part
  operator-(color_part lhs,        // passing lhs by value helps optimize chained a+b+c
            const color_part &rhs) // otherwise, both parameters may be const references
  {
    lhs -= rhs; // reuse compound assignment
    return lhs; // return the result by value (uses move constructor)
  }

  color_part &operator+=(const color_part &rhs) {
    cpv += rhs.cpv;

    return *this;
  }

  // friends defined inside class body are inline and are hidden from non-ADL lookup
  friend color_part
  operator+(color_part lhs,        // passing lhs by value helps optimize chained a+b+c
            const color_part &rhs) // otherwise, both parameters may be const references
  {
    lhs += rhs; // reuse compound assignment
    return lhs; // return the result by value (uses move constructor)
  }

  color_part &operator/=(const color_part &rhs) {
    cpv /= rhs.cpv;
    return *this;
  }

  // friends defined inside class body are inline and are hidden from non-ADL lookup
  friend color_part
  operator/(color_part lhs,        // passing lhs by value helps optimize chained a+b+c
            const color_part &rhs) // otherwise, both parameters may be const references
  {
    lhs /= rhs; // reuse compound assignment
    return lhs; // return the result by value (uses move constructor)
  }

  explicit operator bool() const noexcept { return cpv; }
  explicit operator double() const noexcept { return cpv; }

  auto operator<=>(const color_part &) const = default;

  void assign(const color_part &cp) noexcept { cpv = cp.cpv; }

  template <typename T = TAG> void assign(double v) noexcept {
    if constexpr (std::same_as<T, _hue_tag>) {
      cpv = (v > 360.0) ? v / 360.0 : v;
    } else {
      cpv = (v > 1.0) ? v / 100.0 : v;
    }
  }

  void clear() noexcept { cpv = 0.0; }

  bool valid() const {
    if constexpr (std::same_as<TAG, _hue_tag>) {
      return (cpv >= 0.0) && (cpv <= 360.0);
    } else {
      return (cpv >= 0.0) && (cpv <= 100.0);
    }

    return false;
  }

private:
  double cpv{};
};

using Bri = color_part<_bri_tag>;
using Hue = color_part<_hue_tag>;
using Sat = color_part<_sat_tag>;

constexpr Hue operator""_HUE(long double v) { return v; }
constexpr Sat operator""_SAT(long double v) { return v; }
constexpr Bri operator""_BRI(long double v) { return v; }

} // namespace desk
} // namespace pierre

template <typename T>
  requires pierre::desk::IsSpecializedColorPart<T>
struct fmt::formatter<T> : public fmt::formatter<std::string> {

  template <typename FormatContext>
  auto format(const T &p, FormatContext &ctx) const -> decltype(ctx.out()) {

    std::string msg;
    auto w = std::back_inserter(msg);

    if constexpr (std::same_as<T, pierre::desk::Hue>) {
      fmt::format_to(w, "{:03.01f}", p.cpv);
    } else {
      fmt::format_to(w, "{:03.01f}", p.cpv);
    }

    // write to ctc.out()
    return fmt::format_to(ctx.out(), "{}", msg);
  }
};
