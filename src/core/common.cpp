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
#include <fmt/format.h>

namespace pierre {

using namespace std::chrono_literals;
using std::chrono::steady_clock;
using fpSeconds = std::chrono::duration<double, std::chrono::milliseconds::period>;

typedef std::chrono::steady_clock Clock;

static auto startup = Clock::now();

// Global Helpers Definitions

ccs fnName(csrc_loc loc) { return loc.function_name(); }

const string runTicks() {
  constexpr auto f = FMT_STRING("{:10.2f} ");

  auto diff = fpSeconds(Clock::now() - startup);

  return fmt::format(f, diff.count());
}

} // namespace pierre
