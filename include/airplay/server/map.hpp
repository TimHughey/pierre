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

#pragma once

#include "common/ss_inject.hpp"
#include "server/base.hpp"

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace pierre {
namespace airplay {

typedef std::vector<ServerType> TeardownList;

class Servers;
typedef std::shared_ptr<Servers> shServers;

namespace shared {
std::optional<shServers> &servers();
} // namespace shared

class Servers : public std::enable_shared_from_this<Servers> {
private:
  typedef std::shared_ptr<server::Base> ServerPtr;
  typedef std::map<ServerType, ServerPtr> ServerMap;

public:
  static shServers init(server::Inject di) {
    // must forward di since we want the actual object
    return shared::servers().emplace(new Servers(di));
  }

  static shServers ptr() { return shared::servers().value()->shared_from_this(); }
  static void reset() { shared::servers().reset(); }

  ~Servers() { teardown(); }

  Port localPort(ServerType type);
  void teardown();
  void teardown(ServerType type);

private:
  Servers(server::Inject di) : di(di) {}

  ServerPtr fetch(ServerType type);

private:
  // order dependent based on constructor
  server::Inject di;

  // order independent
  ServerMap map;
};

} // namespace airplay
} // namespace pierre
