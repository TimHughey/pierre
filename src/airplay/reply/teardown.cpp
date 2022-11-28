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
#include "base/headers.hpp"
#include "base/render.hpp"
#include "base/shk.hpp"
#include "base/types.hpp"
#include "mdns/mdns.hpp"
#include "mdns/service.hpp"
#include "reply/dict_keys.hpp"
#include "rtsp/ctx.hpp"
#include "server/servers.hpp"

namespace pierre {
namespace airplay {
namespace reply {

bool Teardown::populate() {
  rdict = plist();

  headers.add(hdr_type::ContentSimple, hdr_val::ConnectionClosed);
  responseCode(RespCode::OK); // always OK

  return rdict.exists(dk::STREAMS) ? phase1() : phase12();
}

bool Teardown::phase1() {
  SharedKey::clear();
  Render::set(false);

  return true;
}

bool Teardown::phase2() { // we've been asked to disconnect
  // auto servers = Servers::ptr().get();

  Service::ptr()->receiverActive(false);
  mDNS::update();

  Servers::teardown();

  di->rtsp_ctx->group_contains_group_leader = false;
  di->rtsp_ctx->active_remote = 0;

  return true;
}

} // namespace reply
} // namespace airplay
} // namespace pierre