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

#include "server/servers.hpp"
#include "base/io.hpp"
#include "server/audio.hpp"
#include "server/control.hpp"
#include "server/event.hpp"
#include "server/rtsp.hpp"

namespace pierre {
namespace airplay {

namespace shared {
std::optional<shServers> __servers;
std::optional<shServers> &servers() { return __servers; }
} // namespace shared

Servers::ServerPtr Servers::fetch(ServerType type) {
  if (map.contains(type) == false) {
    return ServerPtr(nullptr);
  }

  return map.at(type);
}

Port Servers::localPort(ServerType type) {
  auto svr = fetch(type);

  if (svr.get() == nullptr) {
    switch (type) {
    case ServerType::Audio:
      map.emplace(type, new server::Audio(io_ctx));
      break;

    case ServerType::Event:
      map.emplace(type, new server::Event(io_ctx));
      break;

    case ServerType::Control:
      map.emplace(type, new server::Control(io_ctx));
      break;

    case ServerType::Rtsp:
      map.emplace(type, new server::Rtsp(io_ctx));
      break;
    }

    svr = map.at(type);

    svr->asyncLoop(); // start the server
  }

  return svr->localPort();
}

void Servers::teardown() {
  using enum ServerType;

  for (const auto type : std::array{Audio, Event, Control, Rtsp}) {
    teardown(type);
  }
}

void Servers::teardown(ServerType type) {
  auto svr = fetch(type);

  if (svr.get() != nullptr) {
    svr->teardown();
    map.erase(type);
  }
}

} // namespace airplay
} // namespace pierre
