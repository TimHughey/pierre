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
#include "desk/data_msg.hpp"
#include "desk/unit/names.hpp"

namespace pierre {
namespace desk {

constexpr size_t no_frame{0};

class Unit {
public:
  Unit(const hdopts &opts, size_t frame_len = no_frame)
      : name(opts.name),       // need to store an actual string so we can make
        type(opts.type),       // type of unit (switch, dimmable, etc)
        address(opts.address), // abstract address (used by subclasses as needed)
        frame_len(frame_len)   // support headunits that do not use the DMX frame
  {}

  friend class Units;

  virtual void activate() noexcept {}
  virtual void dark() noexcept {}

  // message processing loop
  virtual void prepare() noexcept {}
  virtual void update_msg(DataMsg &msg) noexcept { msg.noop(); }

protected:
  // order dependent
  const string name;
  const string type;
  const uint16_t address;
  const size_t frame_len;
};

} // namespace desk
} // namespace pierre
