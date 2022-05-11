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

#include "fmt/format.h"

namespace pierre {
namespace airplay {
namespace server {

class Base {
public:
  static constexpr uint16_t ANY_PORT = 0;
  static constexpr bool LOG_TRUE = true;
  static constexpr bool LOG_FALSE = false;

public:
  Base() = default;
  virtual ~Base(){};

  virtual void asyncLoop() = 0;
  virtual void asyncLoop([[maybe_unused]] const error_code &ec) {}
  virtual Port localPort() = 0;
  virtual void teardown() = 0;

protected:
  void __infoAccept(csv server_id, const auto handle, bool log = LOG_FALSE) {
    if (log) { // log accepted connection
      constexpr auto f = FMT_STRING("{} {} accepted connection, handle={}\n");
      fmt::print(f, runTicks(), server_id, handle);
    }
  }
};

} // namespace server
} // namespace airplay
} // namespace pierre
