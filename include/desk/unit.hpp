// Pierre
// Copyright (C) 2022 Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#pragma once

#include "base/types.hpp"
#include "desk/msg/data.hpp"
#include "desk/unit/names.hpp"

namespace pierre {
namespace desk {

constexpr size_t no_frame{0};

class Unit {
  friend class Units;

public:
  Unit(string &&name, uint16_t address, size_t frame_len = no_frame)
      : // capture the name of this unit
        name(name),
        // device specific address (e.g. DMX, gpio pin)
        address(address),
        // num of bytes when part of DMX payload
        frame_len{frame_len} {}

  // all units, no matter subclass, must support
  virtual void activate() noexcept {}
  virtual void dark() noexcept {}

  // message processing loop
  virtual void prepare() noexcept {}
  virtual void update_msg(DataMsg &msg) noexcept { msg.noop(); }

protected:
  // order dependent
  string name;
  uint16_t address;
  size_t frame_len;
};

} // namespace desk
} // namespace pierre
