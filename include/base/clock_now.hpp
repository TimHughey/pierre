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

#include "base/qpow10.hpp"

#include <time.h>

namespace pierre {

/// @brief Helper for clock cuurent raw nanoseconds
struct clock_now {

  /// @brief Return raw nanoseconds of clock type
  /// @param clock_type see time.hpp
  /// @return raw nanoseconds
  static int64_t ns_raw(int clock_type) noexcept {
    struct timespec tn;
    clock_gettime(clock_type, &tn);

    return tn.tv_sec * ipow(10, 9) + tn.tv_nsec;
  }

  struct mono {
    static int64_t ns() noexcept { return ns_raw(CLOCK_MONOTONIC_RAW); }
    static int64_t us() noexcept { return ns_raw(CLOCK_MONOTONIC_RAW) / 1000; }
  };

  struct real {
    static int64_t us() noexcept { return ns_raw(CLOCK_REALTIME) / 1000; }
  };
};

/// @brief Free function to return monotonic clock raw nanoseconds
/// @return mono clock raw now nanoseconds
inline auto clock_mono_ns() noexcept { return clock_now::mono::ns(); }

/// @brief free function to return monotonic clock raw microseconds
/// @return mono clock raw noe microseconds
inline auto clock_mono_us() noexcept { return clock_now::mono::us(); }

} // namespace pierre