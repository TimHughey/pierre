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

#include "replies/set_anchor.hpp"
#include "base/logger.hpp"
#include "frame/anchor.hpp"
#include "frame/anchor_data.hpp"
#include "frame/racked.hpp"
#include "replies/dict_kv.hpp"

namespace pierre {
namespace rtsp {

SetAnchor::SetAnchor(Request &request, Reply &reply) noexcept {
  Aplist request_dict(request.content);

  // a complete anchor message contains these keys
  static const Aplist::KeyList keys{NET_TIMELINE_ID, NET_TIME_SECS, NET_TIME_FRAC, NET_TIME_FLAGS,
                                    RTP_TIME};

  if (request_dict.existsAll(keys)) {
    // this is a complete anchor data set
    Anchor::save(                                        // submit the new anchor data
        AnchorData(request_dict.uint({NET_TIMELINE_ID}), // network timeline id (aka source clk)
                   request_dict.uint({NET_TIME_SECS}),   // source clock seconds
                   request_dict.uint({NET_TIME_FRAC}),   // source clock fractional nanos
                   request_dict.uint({RTP_TIME}),        // rtp time (as defined by source),
                   request_dict.uint({NET_TIME_FLAGS})   // flags (from source)
                   ));
  } else {
    Anchor::reset();
  }

  if (request_dict.exists(RATE)) {
    // note: rate is misleading, it is actually the flag that controls playback
    bool rate = request_dict.uint({RATE});
    Racked::spool(rate);
  } else {
    INFO(module_id, "NOTICE", "rate not present\n");
  }

  reply(RespCode::OK);
}

} // namespace rtsp
} // namespace pierre
