// Pierre
// Copyright (C) 2022 Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#pragma once

#include "base/types.hpp"

namespace pierre {
namespace unit {

struct Opts {
  csv name;
  uint8_t address;
};

constexpr size_t NO_FRAME = 0;

constexpr csv AC_POWER{"ac power"};
constexpr csv MAIN_SPOT{"main pinspot"};
constexpr csv FILL_SPOT{"fill pinspot"};
constexpr csv EL_ENTRY{"el entry"};
constexpr csv EL_DANCE{"el dance"};
constexpr csv LED_FOREST{"led forest"};
constexpr csv DISCO_BALL{"disco ball"};

constexpr Opts AC_POWER_OPTS{.name = AC_POWER, .address = 0};
constexpr Opts MAIN_SPOT_OPTS{.name = MAIN_SPOT, .address = 1};
constexpr Opts FILL_SPOT_OPTS{.name = FILL_SPOT, .address = 7};
constexpr Opts EL_ENTRY_OPTS{.name = EL_ENTRY, .address = 1};
constexpr Opts EL_DANCE_OPTS{.name = EL_DANCE, .address = 2};
constexpr Opts LED_FOREST_OPTS{.name = LED_FOREST, .address = 3};
constexpr Opts DISCO_BALL_OPTS{.name = DISCO_BALL, .address = 4};

} // namespace unit
} // namespace pierre
