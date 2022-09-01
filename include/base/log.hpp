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

#include "base/pet.hpp"
#include "base/typical.hpp"

#include <cstdint>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <iterator>
#include <ranges>
#include <source_location>

namespace {
namespace ranges = std::ranges;
}

namespace pierre {

using src_loc = std::source_location;
typedef const src_loc csrc_loc;

constexpr ccs fnName(csrc_loc loc = src_loc::current()) { return loc.function_name(); }
const string runTicks(); // a timestamp

#define __LOG(format, ...) __log(1, FMT_STRING(format), __VA_ARGS__)
#define __LOG0(format, ...) __log(0, FMT_STRING(format), __VA_ARGS__)
#define __LOG1(format, ...) __log(1, FMT_STRING(format), __VA_ARGS__)
#define __LOGX(format, ...)

#define LCOL0 "{:18}"
#define LCOL1 "{:15}"
#define LCOL01 LCOL0 " " LCOL1
#define LBLANK " "

const auto __LOG_COL2 = fmt::format(LCOL01, LBLANK, LBLANK);
csv __LOG_COL2_SV{__LOG_COL2};
const auto __LOG_PREFIX = fmt::format("{:11} ", " ");
const auto __LOG_MODULE_ID_INDENT = fmt::format("\n{}{:18} ", __LOG_PREFIX, " ");
const auto __LOG_DETAIL_INDENT = fmt::format("\n{} " LCOL01, __LOG_PREFIX, LBLANK, LBLANK);

struct pe_log {
  static void indent2(string &msg, csv more) {
    msg.append(__LOG_COL2_SV); // add indent for first line

    ranges::for_each(more, [&msg = msg](const auto c) {
      if (c == '\n') {
        fmt::format_to(std::back_inserter(msg), "\n{}", __LOG_COL2_SV);
      } else {
        msg.push_back(c);
      }
    });
  }
  static void nlCol2(auto &w) { fmt::format_to(w, "\n{}", __LOG_COL2); }

  static void prepend(auto &w, csv module_id, csv category) {
    fmt::format_to(w, LCOL01, module_id, category);
  }
};

void __vlog(fmt::string_view format, fmt::format_args args);

template <typename S, typename... Args> // accepts the format and var args
void __log([[maybe_unused]] int level, const S &format, Args &&...args) {
  __vlog(format, fmt::make_args_checked<Args...>(format, args...));
}

} // namespace pierre
