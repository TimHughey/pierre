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
#include "base/shk.hpp"
#include "base/typical.hpp"
#include "conn_info/conn_info.hpp"
#include "core/service.hpp"
#include "desk/desk.hpp"
#include "mdns/mdns.hpp"
#include "reply/dict_keys.hpp"
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
  __LOGX(LCOL01 " {}\n", moduleID(), csv("PHASE 1"));
  SharedKey::clear();
  idesk()->adjust_mode(Desk::NOT_RENDERING);

  return true;
}

bool Teardown::phase2() { // we've been asked to disconnect
  __LOGX(LCOL01 " {}\n", moduleID(), csv("PHASE 2"));
  auto servers = Servers::ptr().get();

  Service::ptr()->receiverActive(false);
  mDNS::ptr()->update();

  servers->teardown(ServerType::Event);
  servers->teardown(ServerType::Control);
  servers->teardown(ServerType::Audio);

  if (ConnInfo::ptr()->stream.isNtpStream()) {
    ConnInfo::ptr()->airplay_gid.clear();
  }

  ConnInfo::ptr()->groupContainsGroupLeader = false;
  ConnInfo::ptr()->dacp_active_remote.clear();

  idesk()->halt();

  return true;
}

} // namespace reply
} // namespace airplay
} // namespace pierre