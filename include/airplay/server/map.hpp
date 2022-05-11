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

#include <unordered_map>
#include <variant>

namespace pierre {
namespace airplay {

namespace server {

class Map {
private:
  typedef std::shared_ptr<Base> server_ptr;
  typedef std::unordered_map<ServerType, server_ptr> ServerMap;

public:
  Map(const Inject &di);
  ~Map() { teardown(); }

  Port localPort(ServerType type) const;
  const PortMap portList() const;
  void teardown();

private:
  // order dependent based on constructor
  const Inject &di;

  // order independent
  ServerMap map;
};

} // namespace server
} // namespace airplay
} // namespace pierre
