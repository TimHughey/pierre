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

#include "core/typedefs.hpp"

#include <chrono>

namespace pierre {

using namespace std::chrono_literals;

using Micros = std::chrono::microseconds;
using MicrosFP = std::chrono::duration<double, std::chrono::microseconds::period>;
using Millis = std::chrono::milliseconds;
using MillisFP = std::chrono::duration<double, std::chrono::milliseconds::period>;
using Nanos = std::chrono::nanoseconds;
using Seconds = std::chrono::duration<long double>;

namespace rtp_time {

typedef uint64_t ClockID; // clock ID

constexpr Nanos AGE_MAX{10s};
constexpr Nanos AGE_MIN{1500ms};
constexpr Nanos NS_FACTOR{upow(10, 9)};
constexpr Nanos ZERO_NANOS{0};

constexpr Millis from_ms(int64_t ms) { return Millis(ms); }
constexpr Nanos from_ns(uint64_t ns) { return Nanos(ns); }

Millis nowMillis();
Nanos nowNanos();

} // namespace rtp_time

} // namespace pierre
