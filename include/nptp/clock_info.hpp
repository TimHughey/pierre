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

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fmt/format.h>
#include <iterator>
#include <pthread.h>
#include <source_location>
#include <vector>

#include "nptp/shm_struct.hpp"

namespace pierre {
namespace nptp {

class ClockInfo {
public:
  typedef std::vector<uint8_t> Data;

public:
  using Clock = std::chrono::steady_clock;

  typedef uint64_t ClockId;

public:
  ClockInfo(){}; // allow for placeholder objects

  ClockInfo(void *shm_data);

  // clock info (direct access)
  ClockId clockID = 0;
  uint64_t sampleTime = 0;
  uint64_t rawOffset = 0;
  uint64_t mastershipStartTime = 0;

  // refresh the clock sample data
  void refresh(void *shm_data);

  // size of the memory mapped region
  static constexpr size_t mappedSize() { return sizeof(shm_structure); }

  // current time
  uint64_t now() const { return Clock::now().time_since_epoch().count(); }

  // is this sample too old?
  bool tooOld() const { return too_old; }

  void dump() const;

private:
  void __init();

  void copyData();
  void populate();

private:
  static const char *fnName(const std::source_location loc = std::source_location::current()) {
    return loc.function_name();
  }

private:
  bool too_old = false;

private:
  static constexpr auto VERSION = 7;

  void *_shm_data = nullptr;
  Data _nqptp;
};

} // namespace nptp
} // namespace pierre