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

#include "base/io.hpp"
#include "base/logger.hpp"

namespace pierre {
namespace airplay {
namespace server {

class Base {
public:
  static constexpr uint16_t ANY_PORT{0};

public:
  Base(csv server_id) : server_id(server_id){};
  virtual ~Base(){};

  virtual void asyncLoop(const error_code ec_last = error_code()) = 0;
  virtual Port localPort() = 0;
  csv serverId() const { return server_id; }
  virtual void shutdown() { teardown(); }
  virtual void teardown() = 0;

public:
  string server_id; // used for logging
};

} // namespace server
} // namespace airplay
} // namespace pierre
