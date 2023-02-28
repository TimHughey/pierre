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

namespace desk {

struct unit {
  static const string AC_POWER;
  static const string MAIN_SPOT;
  static const string FILL_SPOT;
  static const string EL_ENTRY;
  static const string EL_DANCE;
  static const string LED_FOREST;
  static const string DISCO_BALL;
};

struct unit_type {
  static const string PINSPOT;
  static const string DIMMABLE;
  static const string SWITCH;
  static const string VAR_SPEED;
};

struct hdopts {
  const string name;
  const string type;
  size_t address;
};

} // namespace desk
} // namespace pierre