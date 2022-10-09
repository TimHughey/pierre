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

struct pet {
  static constexpr Nanos NS_FACTOR{upow(10, 9)};

  static constexpr auto abs(const auto &d1) { return std::chrono::abs(d1); }

  static constexpr Nanos add_offset(const Nanos &d, uint64_t offset) {
    return Nanos{to_uint64_t(d) + offset};
  }

  static constexpr Nanos apply_offset(const Nanos &d, uint64_t offset) {
    return Nanos{static_cast<uint64_t>(d.count()) + offset};
  }

  template <typename TO> static constexpr TO as(auto x) {
    return std::chrono::duration_cast<TO>(Nanos(x));
  }

  template <typename FROM, typename TO> static constexpr TO as_duration(auto x) {
    return std::chrono::duration_cast<TO>(FROM(x));
  }

  template <typename T> static MillisFP as_millis_fp(const T &d) {
    return std::chrono::duration_cast<MillisFP>(d);
  }

  template <typename T> static SecondsFP as_secs(const T &d) {
    return std::chrono::duration_cast<SecondsFP>(d);
  }

  template <typename FROM, typename TO> static constexpr TO cast(auto x) {
    return std::chrono::duration_cast<TO>(FROM(x));
  }

  template <typename T> static T diff_abs(const T &d1, const T &d2) {
    return std::chrono::abs(d1 - d2);
  }

  static string humanize(const Nanos d);

  template <typename T> static constexpr bool is_zero(T val) { return val == Nanos::zero(); }

  static Nanos elapsed(const Nanos &d1, const Nanos d2 = pet::now_monotonic()) { return d2 - d1; }

  template <typename T>
  static T elapsed_as(const Nanos &d1, const Nanos d2 = pet::now_monotonic()) {
    return std::chrono::duration_cast<T>(d2 - d1);
  }

  static Nanos elapsed_abs(const Nanos &d1, const Nanos d2 = pet::now_monotonic()) { //
    return diff_abs(d2, d1);
  }

  static constexpr Millis from_ms(int64_t ms) { return Millis(ms); }

  template <typename S> static constexpr auto from_now(S amount) noexcept {
    return steady_clock::now() + amount;
  }

  static constexpr Nanos from_ns(uint64_t ns) { return Nanos(static_cast<int64_t>(ns)); }

  template <typename T> static constexpr bool not_zero(const T &d) { return d != T::zero(); }

  static Nanos now_monotonic() { return now_nanos(); }

  template <typename T = Nanos> static T now_monotonic() {
    return std::chrono::duration_cast<T>(now_nanos());
  }

  template <typename T = Nanos> static T now_epoch() {
    return std::chrono::duration_cast<T>(system_clock::now().time_since_epoch());
  }

  template <typename T> static T now_steady() { return as_duration<Nanos, T>(now_monotonic()); }

  template <typename T = Nanos> static constexpr T percent(T x, int percent) {
    float _percent = percent / 100.0;
    Nanos base = std::chrono::duration_cast<Nanos>(x);
    auto val = Nanos(static_cast<int64_t>(base.count() * _percent));

    return std::chrono::duration_cast<T>(val);
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
  static Nanos now_nanos();
};

} // namespace pierre