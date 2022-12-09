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

#include "reply/teardown.hpp"
#include "airplay/headers.hpp"
#include "base/types.hpp"
#include "frame/racked.hpp"
#include "mdns/mdns.hpp"
#include "reply/dict_keys.hpp"
#include "rtsp/ctx.hpp"

namespace pierre {
namespace airplay {
namespace reply {

bool Teardown::populate() {
  rdict = plist();

  headers.add(hdr_type::ContentSimple, hdr_val::ConnectionClosed);
  resp_code(RespCode::OK); // always OK

  return rdict.exists(dk::STREAMS) ? phase1() : phase12();
}

bool Teardown::phase1() {
  di->rtsp_ctx->shared_key.clear();
  Racked::spool(false);

  return true;
}

bool Teardown::phase2() { // we've been asked to disconnect

  di->rtsp_ctx->service->receiver_active(false);
  mDNS::update();

  di->rtsp_ctx->teardown();

  return true;
}

} // namespace reply
} // namespace airplay
} // namespace pierre