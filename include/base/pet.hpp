//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2022  Tim Hughey
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

#include "base/pet_types.hpp"
#include "base/qpow10.hpp"
#include "base/types.hpp"

#include <concepts>
#include <numeric>
#include <type_traits>

namespace pierre {

struct pet {
  static constexpr auto abs(const auto &d1) { return std::chrono::abs(d1); }

  static constexpr Nanos apply_offset(const Nanos &d, uint64_t offset) {
    return Nanos{static_cast<uint64_t>(d.count()) + offset};
  }

  template <typename TO, typename FROM = Nanos> static constexpr TO as(FROM x) {
    if constexpr (std::same_as<TO, FROM>) {
      return x;
    } else if constexpr (IsDuration<FROM> && IsDuration<TO>) {
      return std::chrono::duration_cast<TO>(x);
    } else if constexpr (std::is_convertible_v<FROM, TO>) {
      return std::chrono::duration_cast<TO>(x);
    } else {
      static_assert(always_false_v<TO>, "unhandled types");
    }
  }

  template <typename T> static T diff_abs(const T &d1, const T &d2) {
    return std::chrono::abs(d1 - d2);
  }

  static string humanize(const Nanos d);

  template <typename T> //
  static constexpr bool is_zero(T val) {
    return val == Nanos::zero();
  }

  static Nanos elapsed(const Nanos &d1, const Nanos d2 = pet::monotonic()) { return d2 - d1; }

  static Nanos elapsed_abs(const Nanos &d1, const Nanos d2 = pet::monotonic()) {
    return diff_abs(d2, d1);
  }

  template <typename AS> static inline AS elapsed_from_raw(auto raw) noexcept {
    return pet::now_monotonic<AS>() - AS(raw);
  }

  template <typename T = Nanos> static T floor(const T &d, const T min = T::zero()) noexcept {
    return d >= min ? d : min;
  }

  template <typename AS, typename BASE = AS, typename T>
  static constexpr AS from_val(T val) noexcept {

    // ensure AS and BASE are chrono durations
    if constexpr (IsDuration<AS> && IsDuration<BASE>) {

      if constexpr (std::is_floating_point_v<T>) {
        // round the value
        int64_t count = static_cast<int64_t>(val);
        return as<AS, BASE>(BASE(count));
      } else if constexpr (std::is_integral_v<T>) {
        int64_t count = static_cast<int64_t>(val);
        return as<AS, BASE>(BASE(count));
      } else if constexpr (std::is_convertible_v<T, BASE>) {
        return as<AS, BASE>(BASE(val));
      } else {
        static_assert(always_false_v<T>, "val must be integral or floating point");
      }
    } else {
      static_assert(always_false_v<T>, "AS and BASE must be chrono durations");
    }
  }

  template <typename T = Millis> static constexpr T from_ms(int64_t ms) noexcept {
    return as<T, Millis>(Millis(ms));
  }

  template <typename T> static constexpr bool not_zero(const T &d) { return d != T::zero(); }

  template <typename T = Nanos> static T now_realtime() { return as<T>(realtime()); }
  template <typename T = Nanos> static T now_monotonic() { return as<T>(monotonic()); }

  template <typename T = Nanos> static constexpr T percent(T x, int val) {
    return percent(x, val / 100.0);
  }

  template <typename T = Nanos> static constexpr T percent(T x, std::floating_point auto val) {
    return T(static_cast<int64_t>(x.count() * val));
  }

  static constexpr Nanos subtract_offset(const Nanos &d, uint64_t offset) {
    return Nanos{static_cast<uint64_t>(d.count()) - offset};
  }

private:
  static Nanos monotonic() noexcept;
  static Nanos realtime() noexcept;
  static Nanos boottime() noexcept;
};

} // namespace pierre
