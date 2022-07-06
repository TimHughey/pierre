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

#include "reply/set_peersx.hpp"
#include "reply/dict_keys.hpp"
#include "rtp_time/clock.hpp"

#include <ranges>

namespace ranges = std::ranges;

namespace pierre {
namespace airplay {
namespace reply {

bool SetPeersX::populate() {
  rdict = plist();

  rdict.dump();

  MasterClock::Peers peer_list;
  const auto count = rdict.arrayItemCount({dk::ROOT});
  __LOGX(LCOL01 " count={}\n", moduleId, csv("POPULATE"), count);

  for (uint32_t idx = 0; idx < count; idx++) {
    auto idxs = fmt::format("{}", idx);
    if (auto peers = rdict.stringArray({csv(idxs), dk::ADDRESSES}); peers.size() > 0) {
      ranges::copy(peers, std::back_inserter(peer_list));

      responseCode(RespCode::OK); // we got some peer addresses
    }
  }

  MasterClock::peers(peer_list);

  return peer_list.size() ? true : false;

  // if (auto peers = rdict.stringArray({dk::IDX0, dk::ADDRESSES}); !peers.empty()) {
  //   MasterClock::peers(peers);  // set the peer lists
  //   responseCode(RespCode::OK); // indicate success
  //   return true;
  // }

  // return false;
}

} // namespace reply
} // namespace airplay
} // namespace pierre
