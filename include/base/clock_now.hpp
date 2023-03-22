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
struct clock_now {

  static int64_t ns_raw(int clock_type) noexcept {
    struct timespec tn;
    clock_gettime(clock_type, &tn);

    return tn.tv_sec * qpow10(9) + tn.tv_nsec;
  }

  struct mono {
    static int64_t ns() noexcept { return ns_raw(CLOCK_MONOTONIC_RAW); }
    static int64_t us() noexcept { return ns_raw(CLOCK_MONOTONIC_RAW) / 1000; }
  };

  struct real {
    static int64_t us() noexcept { return ns_raw(CLOCK_REALTIME) / 1000; }
  };
};

} // namespace pierre