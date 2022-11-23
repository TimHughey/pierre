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

static std::shared_ptr<Servers> servers;

std::shared_ptr<Servers> Servers::init(io_context &io_ctx) noexcept {
  servers = std::shared_ptr<Servers>(new Servers(io_ctx));

  return servers->shared_from_this();
}

Port Servers::localPort(ServerType type) {
  auto svr = fetch(type);

  if (svr.get() == nullptr) {
    switch (type) {
    case ServerType::Audio:
      map.emplace(type, std::make_shared<server::Audio>(io_ctx));
      break;

    case ServerType::Event:
      map.emplace(type, std::make_shared<server::Event>(io_ctx));
      break;

    case ServerType::Control:
      map.emplace(type, std::make_shared<server::Control>(io_ctx));
      break;

    case ServerType::Rtsp:
      map.emplace(type, std::make_shared<server::Rtsp>(io_ctx));
      break;
    }

    svr = map.at(type);

    svr->asyncLoop(); // start the server
  }

  return svr->localPort();
}

std::shared_ptr<Servers> &Servers::self() noexcept { return servers; }

} // namespace airplay
} // namespace pierre
