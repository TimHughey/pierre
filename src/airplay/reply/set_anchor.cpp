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

#include "reply/set_anchor.hpp"
#include "base/anchor_data.hpp"
#include "desk/desk.hpp"
#include "reply/dict_keys.hpp"

namespace pierre {
namespace airplay {
namespace reply {

bool SetAnchor::populate() {
  rdict = plist();

  __LOGX(LCOL01 "\n{}\n", moduleID(), "DICT DUMP", rdict.inspect());

  saveAnchorInfo();

  responseCode(RespCode::OK);

  return true;
}

void SetAnchor::saveAnchorInfo() {
  // a complete anchor message contains these keys
  const Aplist::KeyList keys{dk::RATE,          dk::NET_TIMELINE_ID, dk::NET_TIME_SECS,
                             dk::NET_TIME_FRAC, dk::NET_TIME_FLAGS,  dk::RTP_TIME};

  if (rdict.existsAll(keys)) {
    // this is a complete anchor data set
    idesk()->save_anchor_data(AnchorData{.rate = rdict.uint({dk::RATE}),
                                         .clock_id = rdict.uint({dk::NET_TIMELINE_ID}),
                                         .secs = rdict.uint({dk::NET_TIME_SECS}),
                                         .frac = rdict.uint({dk::NET_TIME_FRAC}),
                                         .flags = rdict.uint({dk::NET_TIME_FLAGS}),
                                         .rtp_time = rdict.uint({dk::RTP_TIME})});
  } else {
    idesk()->save_anchor_data(AnchorData{.rate = rdict.uint({dk::RATE})});
  }
}

} // namespace reply
} // namespace airplay
} // namespace pierre