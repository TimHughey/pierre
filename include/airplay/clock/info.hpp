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

#include "clock/shm/struct.hpp"
#include "common/typedefs.hpp"
#include "core/host.hpp"
#include "core/typedefs.hpp"

#include <array>
#include <chrono>
#include <vector>

namespace pierre {
namespace airplay {

typedef std::vector<string> Peers;
typedef std::chrono::steady_clock SteadyClock;

namespace clock {

// intentionally using namespaces in this header within the clock namespace
using namespace std::chrono;
using namespace std::chrono_literals;
using namespace std::string_literals;

struct Info {
  ClockId clockID = 0;              // current master clock
  MasterClockIp masterClockIp{0};   // IP of master clock
  uint64_t sampleTime = 0;          // time when the offset was calculated
  uint64_t rawOffset = 0;           // master clock time = sampleTime + rawOffset
  uint64_t mastershipStartTime = 0; // when the master clock became maste

  bool ok(auto age = 10s) const;
  static uint64_t now();

  // misc debug
  void dump() const;
};

} // namespace clock
} // namespace airplay
} // namespace pierre