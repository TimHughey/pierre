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

#include "airplay/common/typedefs.hpp"

#include <memory>
#include <optional>

namespace pierre {

class Airplay;
typedef std::shared_ptr<Airplay> shAirplay;

namespace shared {
std::optional<shAirplay> &airplay();
} // namespace shared

class Airplay : public std::enable_shared_from_this<Airplay> {
public:
  // shared instance management
  static shAirplay init() { return shared::airplay().emplace(new Airplay()); }
  static shAirplay ptr() { return shared::airplay().value()->shared_from_this(); }
  static void reset() { shared::airplay().reset(); }

  // thread maintenance
  Thread &run();

  void teardown(){};

private:
  Airplay(){};
};

} // namespace pierre
