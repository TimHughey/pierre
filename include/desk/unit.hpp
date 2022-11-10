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
#include "desk/unit/opts.hpp"

namespace pierre {

class Unit : public std::enable_shared_from_this<Unit> {
public:
  Unit(const unit::Opts opts)
      : name(opts.name),       // need to store an actual string so we can make csv
        address(opts.address), // abstract address (used by subclasses as needed)
        frame_len(0)           // support headunits that do not use the DMX frame
  {}

  Unit(const unit::Opts opts, size_t frame_len)
      : name(opts.name),       // need to store an actual string so we can make csv
        address(opts.address), // abstract address (used by subclasses as needed)
        frame_len(frame_len)   // DMX frame length
  {}

  virtual ~Unit() {}

  friend class Units;

  virtual void dark() = 0;
  virtual void leave() = 0;
  virtual void prepare() = 0;
  virtual void update_msg(desk::DataMsg &msg) = 0;

protected:
  // order dependent
  const string name;
  const uint16_t address;
  const size_t frame_len;
};

} // namespace pierre
