
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

#include "reply/cmd.hpp"
#include "reply/reply.hpp"

namespace pierre {
namespace airplay {
namespace reply {

using namespace packet;

bool Command::populate() {
  auto rc = true;
  rdict = plist();

  if (false) { // debug
    rdict.dump(fnName());
  }

  // default to OK
  responseCode(packet::RespCode::OK);

  if (!rdict.empty() && checkUpdateSupportedCommands()) {
    rc = true;
  }

  return rc;
}

bool Command::checkUpdateSupportedCommands() {
  auto rc = true;

  if (rdict.compareString("type", "updateMRSupportedCommands")) {
    // anytime type=updateMRSupportedCommands say this is a bad request
    responseCode(RespCode::BadRequest);
  }

  return rc;
}

} // namespace reply
} // namespace airplay
} // namespace pierre