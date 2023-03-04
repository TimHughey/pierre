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

#include <cinttypes>
#include <string>

namespace pierre {

namespace desk {

namespace unit {
constexpr std::string AC_POWER{"ac power"};
constexpr std::string MAIN_SPOT{"main pinspot"};
constexpr std::string FILL_SPOT{"fill pinspot"};
constexpr std::string EL_ENTRY{"el entry"};
constexpr std::string EL_DANCE{"el dance"};
constexpr std::string LED_FOREST{"led forest"};
constexpr std::string DISCO_BALL{"disco ball"};
} // namespace unit

namespace unit_type {
constexpr std::string PINSPOT{"pinspot"};
constexpr std::string DIMMABLE{"dimmable"};
constexpr std::string SWITCH{"switch"};
}; // namespace unit_type

struct hdopts {
  const std::string name;
  const std::string type;
  std::size_t address;
};

} // namespace desk
} // namespace pierre