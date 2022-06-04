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

#include <cstdint>
#include <source_location>
#include <string>
#include <string_view>
#include <thread>

namespace pierre {

using src_loc = std::source_location;
typedef const src_loc csrc_loc;

// string, string_view and const char *
using string = std::string;
typedef const string &csr;

using string_view = std::string_view;
typedef const string_view csv;
typedef const char *ccs;

// threads
typedef std::jthread Thread;

// Global Helpers
ccs fnName(csrc_loc loc = src_loc::current()); // func name of the caller or a src_loc
const string runTicks();                       // a timestamp

constexpr uint64_t upow(uint64_t base, uint64_t exp) {
  uint64_t result = 1;

  for (;;) {
    if (exp & 1)
      result *= base;
    exp >>= 1;
    if (!exp)
      break;
    base *= base;
  }

  return result;
}

} // namespace pierre
