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
namespace color {
template <typename T> struct part;
}
} // namespace desk
} // namespace pierre

template <typename T> // declaration
double operator+(double, const pierre::desk::color::part<T> &);

namespace pierre {
namespace desk {
namespace color {

struct brightness_tag {};
struct hue_tag {};
struct saturation_tag {};

template <typename T>
concept HasPartVal = requires(T v) { v.val; };

template <typename T>
concept HasPartValOrDouble = HasPartVal<T> || std::same_as<T, double>;

template <typename T>
concept IsBriOrSat = IsAnyOf<T, brightness_tag, saturation_tag>;

template <typename T>
concept IsColorPartTag = IsAnyOf<T, hue_tag, saturation_tag, brightness_tag>;

template <class TAG> struct part {
  using tag_type = TAG;

  friend fmt::formatter<part<TAG>>;

  constexpr part() noexcept : val{0} {}
  constexpr part(double v) noexcept : val{v} {}

  template <typename T>
    requires HasPartVal<T>
  part(const T &other) noexcept {
    val = other.val;
  }

  template <typename T>
    requires HasPartVal<T>
  part(T &&other) noexcept {
    std::swap(*this, other);
  }

  double &as() noexcept { return val; }

  void clear() noexcept { val = 0.0; }

  static constexpr auto desc() noexcept {
    if constexpr (std::same_as<TAG, hue_tag>) return string("hue");
    if constexpr (std::same_as<TAG, saturation_tag>) return string("sat");

    return string("bri");
  }

  explicit operator bool() const noexcept { return val != 0.0; }
  operator double() const noexcept { return val; }

  // template <typename T>
  //   requires HasPartVal<T>
  // T &operator=(const double &v) noexcept {
  //   val = v;
  //   return *this;
  // }

  template <typename T>
    requires HasPartVal<T>
  T &operator=(T &other) noexcept {
    val = other.val;

    return *this;
  }

  template <typename T>
    requires HasPartValOrDouble<T>
  auto &operator=(const T &v) noexcept {
    if constexpr (std::same_as<T, TAG>)
      val = v.val;
    else if constexpr (std::same_as<T, double>)
      val = v;

    return *this;
  }

  template <typename T>
    requires HasPartVal<T>
  T &operator=(T &&other) noexcept {
    // guard against self moves
    if (&other == this) return *this;

    std::swap(val, other.val);

    return *this;
  }

  template <typename T>
    requires HasPartValOrDouble<T>
  bool operator<(const T &rhs) const {
    if constexpr (HasPartVal<T>) {
      return val < rhs.val;
    } else if constexpr (std::same_as<T, double>) {
      return val < rhs;
    }
  }

  template <typename T>
    requires HasPartValOrDouble<T>
  bool operator>(const T &rhs) const {
    if constexpr (HasPartVal<T>) {
      return val > rhs.val;
    } else if constexpr (std::same_as<T, double>) {
      return val > rhs;
    }
  }

  template <typename T>
    requires HasPartValOrDouble<T>
  bool operator<=(const T &rhs) const {
    if constexpr (HasPartVal<T>) {
      return !(val > rhs.val);
    } else if constexpr (std::same_as<T, double>) {
      return !(val > rhs);
    }
  }

  template <typename T>
    requires HasPartValOrDouble<T>
  bool operator>=(const T &rhs) const {
    if constexpr (HasPartVal<T>) {
      return !(val < rhs.val);
    } else if constexpr (std::same_as<T, double>) {
      return !(val < rhs);
    }
  }

  template <typename T>
    requires HasPartValOrDouble<T>
  bool operator==(const T &rhs) const {
    if constexpr (HasPartVal<T>) {
      return val == rhs.val;
    } else if constexpr (std::same_as<T, double>) {
      return val == rhs;
    }
  }

  template <typename T>
    requires HasPartValOrDouble<T>
  bool operator!=(const T &rhs) const {
    if constexpr (HasPartVal<T>) {
      return val != rhs.val;
    } else if constexpr (std::same_as<T, double>) {
      return val != rhs;
    }
  }

  template <typename T>
    requires HasPartValOrDouble<T>
  auto &operator+=(const T &rhs) noexcept {
    if constexpr (HasPartVal<T>) {
      val += rhs.val;
    } else if constexpr (std::same_as<T, double>) {
      val += rhs;
    }

    return *this;
  }

  template <typename T>
    requires HasPartValOrDouble<T>
  auto &operator-=(const T &rhs) noexcept {
    if constexpr (HasPartVal<T>) {
      val -= rhs.val;
    } else if constexpr (std::same_as<T, double>) {
      val -= rhs;
    }

    return *this;
  }

  template <typename T>
    requires HasPartValOrDouble<T>
  auto &operator*=(const T &rhs) noexcept {
    if constexpr (HasPartVal<T>) {
      val *= rhs.val;
    } else if constexpr (std::same_as<T, double>) {
      val *= rhs;
    }

    return *this;
  }

  template <typename T>
    requires HasPartValOrDouble<T>
  auto &operator/=(const T &rhs) noexcept {
    if constexpr (HasPartVal<T>) {
      val /= rhs.val;
    } else if constexpr (std::same_as<T, double>) {
      val /= rhs;
    }

    return *this;
  }

  void normalize() noexcept { normalize(val); }

  void normalize(auto v) noexcept {
    if constexpr (std::same_as<TAG, color::hue_tag>) {
      val = (v > 360.0) ? v / 360.0 : v;
    } else if constexpr (IsBriOrSat<TAG>) {
      val = (v > 1.0) ? v / 100.0 : v;
    }
  }

  bool valid() const {
    if constexpr (std::same_as<TAG, color::hue_tag>) {
      return (val >= 0.0) && (val <= 360.0);
    } else if constexpr (IsBriOrSat<TAG>) {
      return (val >= 0.0) && (val <= 100.0);
    }

    return false;
  }

  static auto zero() noexcept { return part(0.0); }

  friend double operator+<>(double, const part &);

private:
  double val{};
};

} // namespace color

using hue_t = color::part<color::hue_tag>;
using saturation_t = color::part<color::saturation_tag>;
using brightness_t = color::part<color::brightness_tag>;

} // namespace desk

constexpr desk::hue_t operator""_hue(long double v) { return desk::hue_t(v); }

} // namespace pierre

namespace {
namespace desk = pierre::desk;
}

template <typename T>
concept IsHsbPart = pierre::IsAnyOf<T, desk::hue_t, desk::saturation_t, desk::brightness_t>;

template <typename T>
  requires IsHsbPart<T>
struct fmt::formatter<T> : public fmt::formatter<std::string> {

  template <typename FormatContext>
  auto format(const T &p, FormatContext &ctx) const -> decltype(ctx.out()) {

    std::string msg;
    auto w = std::back_inserter(msg);

    if constexpr (std::same_as<T, desk::hue_t>) {
      fmt::format_to(w, "{:03.01f}", p.val);
    } else if constexpr (pierre::IsAnyOf<T, desk::saturation_t, desk::brightness_t>) {
      fmt::format_to(w, "{:03.01f}", p.val);
    }

    // write to ctc.out()
    return fmt::format_to(ctx.out(), "{}", msg);
  }
};

template <typename T> double operator+(double v, const pierre::desk::color::part<T> &p) {
  return v + p.val;
}
