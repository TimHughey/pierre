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
#include "conn_info/conn_info.hpp"
#include "core/service.hpp"
#include "mdns/mdns.hpp"
#include "packet/headers.hpp"
#include "reply/dict_keys.hpp"
#include "server/servers.hpp"

namespace pierre {
namespace airplay {
namespace reply {

namespace header = pierre::packet::header;

bool Teardown::populate() {
  auto servers = Servers::ptr();
  rdict = plist();

  headers.add(header::type::ContentSimple, header::val::ConnectionClosed);
  responseCode(packet::RespCode::OK); // always OK

  auto has_streams = rdict.exists(dk::STREAMS);

  if (has_streams == true) { // stop processing audio data
    if (false) {             // debug
      constexpr auto f = FMT_STRING("{} {} phase 1 has_streams={}\n");
      fmt::print(f, runTicks(), fnName(), has_streams);
    }

    phase1();

  } else { // we've been asked to disconnect

    if (false) { // debug
      constexpr auto f = FMT_STRING("{} {} phase 2 has_streams={}\n");
      fmt::print(f, runTicks(), fnName(), has_streams);
    }

    phase1();
    phase2();
  }

  return true;
}

void Teardown::phase1() { ConnInfo::ptr()->sessionKeyClear(); }

void Teardown::phase2() {
  Service::ptr()->receiverActive(false);
  mDNS::ptr()->update();

  Servers::ptr()->teardown(ServerType::Event);
  Servers::ptr()->teardown(ServerType::Control);
  Servers::ptr()->teardown(ServerType::Audio);

  if (ConnInfo::ptr()->stream.isNtpStream()) {
    ConnInfo::ptr()->airplay_gid.clear();
  }
  ConnInfo::ptr()->groupContainsGroupLeader = false;
  ConnInfo::ptr()->dacp_active_remote.clear();
}

} // namespace reply
} // namespace airplay
} // namespace pierre