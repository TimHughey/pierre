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
#include <fmt/chrono.h>
#include <fmt/format.h>
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

// misc debug
const auto __LOG_PREFIX = fmt::format("{:11} ", " ");
const auto __LOG_MODULE_ID_INDENT = fmt::format("\n{}{:18} ", __LOG_PREFIX, " ");
void __vlog(fmt::string_view format, fmt::format_args args);

template <typename S, typename... Args> // accepts the format and var args
void __log([[maybe_unused]] int level, const S &format, Args &&...args) {
  __vlog(format, fmt::make_args_checked<Args...>(format, args...));
}

#define __LOG(format, ...) __log(1, FMT_STRING(format), __VA_ARGS__)
#define __LOG0(format, ...) __log(0, FMT_STRING(format), __VA_ARGS__)
#define __LOG1(format, ...) __log(1, FMT_STRING(format), __VA_ARGS__)
#define __LOGX(format, ...)

} // namespace pierre
