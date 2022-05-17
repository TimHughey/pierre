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

#include "airplay/airplay.hpp"
#include "airplay/controller.hpp"

#include <chrono>
#include <list>
#include <optional>
#include <thread>

namespace pierre {

namespace shared {
std::optional<shAirplay> __airplay;
std::optional<shAirplay> &airplay() { return shared::__airplay; }
} // namespace shared

Thread &Airplay::run() {
  auto controller = airplay::Controller::init();

  return controller->start();
}

} // namespace pierre
