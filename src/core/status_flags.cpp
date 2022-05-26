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

#include "status_flags.hpp"

#include <array>

namespace pierre {

using namespace sf;

StatusFlags::StatusFlags() {
  // default is always audio cable attached
  _flags.set(AudioLink);
}

StatusFlags &StatusFlags::ready() {
  _flags.set(AudioLink);
  _flags.reset(RemoteControlRelay);
  _flags.reset(ReceiverSessionIsActive);

  return *this;
}

StatusFlags &StatusFlags::playing() {
  _flags.set(AudioLink);
  _flags.set(RemoteControlRelay);
  _flags.set(ReceiverSessionIsActive);

  return *this;
}

} // namespace pierre