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

#include "server/map.hpp"
#include "server/audio.hpp"
#include "server/control.hpp"
#include "server/event.hpp"
#include "server/rtsp.hpp"

namespace pierre {
namespace airplay {
namespace server {

/*
Audio &Map::audio() {
  auto &variant = getOrCreate(ServerType::Audio);

  return std::get<server::Audio>(variant);
}

Control &Map::control() {
  auto &variant = getOrCreate(ServerType::Control);

  return std::get<server::Control>(variant);
}

Event &Map::event() {
  auto &variant = getOrCreate(ServerType::Event);

  return std::get<server::Event>(variant);
}

Rtsp &Map::rtsp() {
  auto &variant = getOrCreate(ServerType::Rtsp);

  return std::get<server::Rtsp>(variant);
}

*/

Map::Map(const Inject &di) : di(di) {
  map.emplace(ServerType::Audio, std::make_shared<Audio>(di));
  map.emplace(ServerType::Control, std::make_shared<Control>(di));
  map.emplace(ServerType::Event, std::make_shared<Event>(di));
  map.emplace(ServerType::Rtsp, std::make_shared<Rtsp>(di));
}

Port Map::localPort(ServerType type) const {
  map.at(type)->asyncLoop();

  return map.at(type)->localPort();
}

/*
Port Map::localPort(ServerType type) {
  switch (type) {
    case ServerType::Audio:
      map.try_emplace(type, std::make_shared<Audio>(di));
      break;

    case ServerType::Control:
      map.try_emplace(type, std::make_shared<Control>(di));
      break;

    case ServerType::Event:
      map.try_emplace(type, std::make_shared<Event>(di));
      break;

    case ServerType::Rtsp:
      map.try_emplace(type, std::make_shared<Rtsp>(di));
      break;
  }

  auto server_ptr = map.at(type);

  return server_ptr->localPort();
}


Map::Variant &Map::getOrCreate(ServerType type) {
  Port port = 0;

  switch (type) {
    case ServerType::Audio:
      map.try_emplace(type, Variant(server::Audio(di)));
      break;

    case ServerType::Control:
      map.try_emplace(type, Variant(server::Control(di)));
      break;

    case ServerType::Event:
      map.try_emplace(type, Variant(server::Event(di)));
      break;

    case ServerType::Rtsp:
      map.try_emplace(type, Variant(server::Rtsp(di)));
      break;

    case ServerType::NoServer:
      break;
  }
}
*/

const PortMap Map::portList() const {
  PortMap port_map;

  for (auto &[type, server] : map) {
    port_map.emplace(type, localPort(type));
  }

  return port_map;
}

void Map::teardown() {
  for (auto &entry : map) {
    auto &[type, server] = entry;

    server->teardown();
  }

  map.clear();
}

// void Map::teardown() {
//   for (auto &[key, variant] : map) {
//     std::visit([](auto &&server) { server.teardown(); }, variant);
//   }

//   map.clear();
// }

} // namespace server
} // namespace airplay
} // namespace pierre