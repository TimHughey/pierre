/*
    Pierre - Custom Light Show for Wiss Landing
    Copyright (C) 2022  Tim Hughey

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

#include "reply/set_peers.hpp"
#include "base/logger.hpp"
#include "frame/master_clock.hpp"
#include "reply/dict_keys.hpp"

namespace pierre {
namespace airplay {
namespace reply {

bool SetPeers::populate() {
  rdict = plist();

  INFOX(REPLY_TYPE, "RDICT", "{}\n", rdict.inspect());

  auto peers = rdict.stringArray({dk::ROOT});
  if (peers.empty()) {
    return false;
  }

  shared::master_clock->peers(peers); // set the peer lists
  responseCode(RespCode::OK);         // indicate success
  return true;
}

} // namespace reply
} // namespace airplay
} // namespace pierre
