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

#include "log.hpp"

#include <chrono>
#include <fmt/chrono.h>
#include <fmt/compile.h>
#include <fmt/format.h>

namespace pierre {
static auto startup = pet::now_monotonic();

// Global Helpers Definitions

const string runTicks() { //
  return fmt::format("{:>11.1} ", pet::elapsed_as<MillisFP>(startup));
}

void __vlog(fmt::string_view format, fmt::format_args args) {
  fmt::print("{:>11.1} ", pet::elapsed_as<MillisFP>(startup));
  fmt::vprint(format, args);
}

} // namespace pierre
