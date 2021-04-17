/*
    Pierre - Custom Light Show for Wiss Landing
    Copyright (C) 2021  Tim Hughey

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

#include "lightdesk/fx/fx.hpp"

using namespace std;
using namespace std::string_view_literals;

namespace pierre {
namespace lightdesk {
namespace fx {

std::shared_ptr<HeadUnitTracker> Fx::_tracker;

void Fx::execute(const Peaks peaks) {

  State::silent(peaks->silence());

  executeFx(move(peaks));
}

bool Fx::matchName(const string match) {
  auto rc = false;
  if (name().compare(match) == 0) {
    rc = true;
  }

  return rc;
}

void Fx::setTracker(std::shared_ptr<HeadUnitTracker> tracker) {
  _tracker = std::move(tracker);
}

} // namespace fx
} // namespace lightdesk
} // namespace pierre
