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

#include "typedefs.hpp"

#include <chrono>
#include <fmt/chrono.h>
#include <fmt/compile.h>
#include <fmt/format.h>

namespace pierre {

using namespace std::chrono_literals;
using namespace std::chrono;
using MillisFP = duration<long double, milliseconds::period>;

static auto startup = steady_clock::now();

// Global Helpers Definitions

ccs fnName(csrc_loc loc) { return loc.function_name(); }

const string runTicks() {
  return fmt::format("{:>11.1} ", duration_cast<MillisFP>(steady_clock::now() - startup));
}

// misc debug
void __vlog(fmt::string_view format, fmt::format_args args) {
  fmt::print("{:>11.1}  ", duration_cast<MillisFP>(steady_clock::now() - startup));
  fmt::vprint(format, args);
}

} // namespace pierre
