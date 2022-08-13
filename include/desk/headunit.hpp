/*
    Custom Light Show for Wiss Landing
    Copyright (C) 2020  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#pragma once

#include "base/input_info.hpp"
#include "base/typical.hpp"
#include "desk/msg.hpp"
#include "desk/unit/opts.hpp"

#include <algorithm>
#include <memory>
#include <ranges>
#include <vector>

namespace pierre {

class HeadUnit;
typedef std::shared_ptr<HeadUnit> shHeadUnit;

class HeadUnit {
public:
  HeadUnit(const unit::Opts opts) : unit_name(opts.name), address(opts.address), frame_len(0) {
    // support headunits that do not use the DMX frame
  }

  HeadUnit(const unit::Opts opts, size_t frame_len)
      : unit_name(opts.name), address(opts.address), frame_len(frame_len){};

  virtual ~HeadUnit() {}

  virtual void dark() = 0;
  virtual void prepare() = 0;
  virtual void update_msg(desk::shMsg msg) = 0;
  virtual void leave() = 0;
  csv unitName() const { return csv(unit_name); }

protected:
  // order dependent
  const string unit_name;
  const uint16_t address;
  const size_t frame_len;
};

} // namespace pierre
