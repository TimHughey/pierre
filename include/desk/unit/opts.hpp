/*
    lightdesk/headunits/discoball.hpp - Ruth LightDesk Headunit Disco Ball
    Copyright (C) 2020  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#pragma once

#include "base/types.hpp"

#include <string>

namespace pierre {
namespace unit {

using namespace std::literals::string_view_literals;

struct Opts {
  csv name;
  uint8_t address;
};

constexpr size_t NO_FRAME = 0;

constexpr auto MAIN_SPOT = "main pinspot"sv;
constexpr auto FILL_SPOT = "fill pinspot"sv;
constexpr auto EL_ENTRY = "el entry"sv;
constexpr auto EL_DANCE = "el dance"sv;
constexpr auto LED_FOREST = "led forest"sv;
constexpr auto DISCO_BALL = "disco ball"sv;

constexpr auto MAIN_SPOT_OPTS = Opts{.name = MAIN_SPOT, .address = 0};
constexpr auto FILL_SPOT_OPTS = Opts{.name = FILL_SPOT, .address = 7};
constexpr auto EL_ENTRY_OPTS = Opts{.name = EL_ENTRY, .address = 1};
constexpr auto EL_DANCE_OPTS = Opts{.name = EL_DANCE, .address = 2};
constexpr auto LED_FOREST_OPTS = Opts{.name = LED_FOREST, .address = 3};
constexpr auto DISCO_BALL_OPTS = Opts{.name = DISCO_BALL, .address = 4};

} // namespace unit
} // namespace pierre
