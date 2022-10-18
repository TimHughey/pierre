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
#include "frame/racked.hpp"
#include "reply/dict_keys.hpp"

namespace pierre {
namespace airplay {
namespace reply {

bool SetAnchor::populate() {
  rdict = plist();

  INFOX(moduleID(), "DICT DUMP", "\n{}\n", rdict.inspect());

  auto render = static_cast<bool>(rdict.uint({dk::RATE}) & 0x01);

  // a complete anchor message contains these keys
  const Aplist::KeyList keys{dk::NET_TIMELINE_ID, dk::NET_TIME_SECS, dk::NET_TIME_FRAC,
                             dk::NET_TIME_FLAGS, dk::RTP_TIME};

  if (rdict.existsAll(keys)) {
    // this is a complete anchor data set
    Racked::anchor_save(                              // submit the new anchor data
        render,                                       // rate: 1==render, 0==not render
        AnchorData(rdict.uint({dk::NET_TIMELINE_ID}), // network timeline id (aka source clk)
                   rdict.uint({dk::NET_TIME_SECS}),   // source clock seconds
                   rdict.uint({dk::NET_TIME_FRAC}),   // source clock fractional nanos
                   rdict.uint({dk::RTP_TIME}),        // rtp time (as defined by source),
                   rdict.uint({dk::NET_TIME_FLAGS})   // flags (from source)
                   ));
  } else {
    Racked::anchor_save(render, AnchorData());
  }

  responseCode(RespCode::OK);

  return true;
}

} // namespace reply
} // namespace airplay
} // namespace pierre