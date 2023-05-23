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

#include "base/clock_now.hpp"
#include "base/dura_t.hpp"
#include "base/qpow10.hpp"
#include "base/types.hpp"

namespace pierre {

/// @brief Duration Helpers
struct dura {

  /// @brief Apply raw offset to duration
  /// @tparam TO chrono duration type
  /// @tparam O Integral
  /// @param d Duration to apply offset to
  /// @param offset Offset to apply
  template <typename TO, typename O>
  static constexpr void apply_offset(TO &d, const O &offset) noexcept {
    if constexpr (IsDuration<TO> && std::unsigned_integral<O>) {
      d = Nanos(static_cast<O>(d.count()) + offset);
    } else if constexpr (IsDuration<TO> && std::signed_integral<O>) {
      d = Nanos(d.count() + offset);
    } else {
      static_assert(AlwaysFalse<TO>, "unsupported types");
    }
  }

  /// @brief Apply raw offset to reference and set duration
  /// @tparam TO chrono duration type
  /// @tparam O Integral
  /// @param r Destination duration
  /// @param d2 Duration to apply offset to
  /// @param offset Offset to apply
  template <typename TO, typename O>
  static constexpr void apply_offset(TO &r, const TO &d2, const O &offset) noexcept {
    if constexpr (IsDuration<TO> && std::unsigned_integral<O>) {
      r = Nanos(static_cast<O>(d2.count()) - offset);
    } else if constexpr (IsDuration<TO> && std::signed_integral<O>) {
      r = Nanos(d2.count() - offset);
    } else {
      static_assert(AlwaysFalse<TO>, "unsupported types");
    }
  }

  /// @brief Create duration from a value
  /// @tparam TO Chrono duration type to return
  /// @tparam FROM Chrono duration or type one is constructible from
  /// @param x Value
  /// @return Chrono duration
  template <typename TO, typename FROM = Nanos> static constexpr TO as(FROM x) {
    if constexpr (std::same_as<TO, FROM>) {
      return x;
    } else if constexpr (IsDuration<FROM> && IsDuration<TO>) {
      return std::chrono::duration_cast<TO>(x);
    } else if constexpr (std::constructible_from<TO, FROM>) {
      return TO(x);
    } else {
      static_assert(AlwaysFalse<TO>, "unhandled type");
      return TO(0);
    }
  }

  /// @brief Calculate elapsed time from mono clock now
  /// @tparam D Duration type
  /// @param d1 Value to subtract from mono clock now
  /// @return Elapsed chrono duration
  template <typename D = Nanos> static constexpr D elapsed_abs(D d1) {
    d1 -= dura::as<D>(clock_mono_ns());

    return std::chrono::abs(d1);
  }

  /// @brief Calculate elapsed time from mono clock now
  /// @tparam D Duration type to return
  /// @param raw raw nanoseconds to subtract from mono clock now
  /// @return Elapsed chrono duration
  template <typename D> static constexpr D elapsed_from_raw(auto raw) noexcept {
    int64_t now{0};

    if constexpr (std::same_as<D, Nanos>) {
      now = clock_mono_ns();
    } else if constexpr (std::same_as<D, Micros>) {
      now = clock_mono_us();
    } else {
      static_assert(AlwaysFalse<D>, "unsupported type");
    }

    return std::chrono::abs(D(now - raw));
  }

  /// @brief Convert a chrono duration to human readable string
  /// @param d Duration to convert
  /// @return string
  static string humanize(const Nanos d);

  /// @brief Return a duration type representing mono clock now
  /// @tparam T Duration type to return
  /// @return Duration representing mono clock now
  template <typename T = Nanos> static T now_monotonic() { return as<T>(clock_mono_ns()); }
};

} // namespace pierre
