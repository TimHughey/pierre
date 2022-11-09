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

#include "base/helpers.hpp"
#include "base/types.hpp"

#include <chrono>
#include <concepts>
#include <numeric>
#include <type_traits>

namespace pierre {

using namespace std::chrono_literals;

using Days = std::chrono::days;
using Hours = std::chrono::hours;
using Micros = std::chrono::microseconds;
using MicrosFP = std::chrono::duration<double, std::chrono::microseconds::period>;
using Millis = std::chrono::milliseconds;
using MillisFP = std::chrono::duration<double, std::chrono::milliseconds::period>;
using Minutes = std::chrono::minutes;
using Nanos = std::chrono::nanoseconds;
using Seconds = std::chrono::seconds;
using SecondsFP = std::chrono::duration<long double>;
using steady_clock = std::chrono::steady_clock;
using system_clock = std::chrono::system_clock;
using system_timepoint = std::chrono::time_point<system_clock, Nanos>;

typedef uint64_t ClockID; // master clock id

template <typename T>
concept Integer = std::is_integral<int>::value;

struct pet {
  static constexpr Nanos NS_FACTOR{upow(10, 9)};

  static constexpr auto abs(const auto &d1) { return std::chrono::abs(d1); }

  static constexpr Nanos apply_offset(const Nanos &d, uint64_t offset) {
    return Nanos{static_cast<uint64_t>(d.count()) + offset};
  }

  template <typename TO, typename FROM = Nanos> static constexpr TO as(FROM x) {
    if constexpr (std::is_same_v<TO, FROM>) return x;
    if constexpr (std::is_convertible_v<FROM, TO>) return std::chrono::duration_cast<TO>(x);

    return std::chrono::duration_cast<TO>(FROM(x));
  }

  template <typename T> static T diff_abs(const T &d1, const T &d2) {
    return std::chrono::abs(d1 - d2);
  }

  static string humanize(const Nanos d);

  template <typename T> static constexpr bool is_zero(T val) { return val == Nanos::zero(); }

  static Nanos elapsed(const Nanos &d1, const Nanos d2 = pet::_monotonic()) { return d2 - d1; }

  template <typename T> static T elapsed_as(const Nanos &d1, const Nanos d2 = pet::_monotonic()) {
    return std::chrono::duration_cast<T>(d2 - d1);
  }

  static Nanos elapsed_abs(const Nanos &d1, const Nanos d2 = pet::_monotonic()) {
    return diff_abs(d2, d1);
  }

  template <typename T = Nanos> static T floor(const T &d, const T min = T::zero()) noexcept {
    return d >= min ? d : min;
  }

  static constexpr Millis from_ms(int64_t ms) { return Millis(ms); }

  template <typename S> static constexpr auto from_now(S amount) noexcept {
    return steady_clock::now() + amount;
  }

  static constexpr Nanos from_ns(uint64_t ns) { return Nanos(static_cast<int64_t>(ns)); }

  template <typename T> static constexpr bool not_zero(const T &d) { return d != T::zero(); }

  template <typename T = Nanos> static T now_realtime() { return as<T, Nanos>(_realtime()); }
  template <typename T = Nanos> static T now_monotonic() { return as<T>(_monotonic()); }

  template <typename T = Nanos> static constexpr T percent(T x, int val) {
    return percent(x, val / 100.0);
  }

  template <typename T = Nanos> static constexpr T percent(T x, std::floating_point auto val) {
    return T(static_cast<int64_t>(x.count() * val));
  }

  static constexpr Nanos &reduce(Nanos &val, const Nanos by, const Nanos floor = Nanos::zero()) {
    Nanos rval = val - by;

    val = (rval >= floor) ? rval : floor;

    return val;
  }

  template <typename T = Nanos> static constexpr T reference() {
    return std::chrono::duration_cast<T>(steady_clock::now().time_since_epoch());
  }

  template <typename T = Nanos> static constexpr void set_if_zero(T &to_set, const T val) {
    if (to_set == Nanos::zero()) {
      to_set = val;
    }
  }

  static constexpr Nanos subtract_offset(const Nanos &d, uint64_t offset) {
    return Nanos(to_uint64_t(d) - offset);
  }

  static constexpr uint64_t to_uint64_t(const Nanos &d) { return static_cast<uint64_t>(d.count()); }

private:
  static Nanos _monotonic();
  static Nanos _realtime();
  static Nanos _boottime();
};

} // namespace pierre
