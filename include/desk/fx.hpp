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
#include "desk/units.hpp"
#include "frame/frame.hpp"
#include "fx/names.hpp"

#include <initializer_list>
#include <memory>

namespace pierre {

class FX;
typedef std::shared_ptr<FX> shFX;

struct fx_factory {
  template <typename T> static shFX create() {
    return std::static_pointer_cast<T>(std::make_shared<T>());
  }

  template <typename T> static std::shared_ptr<T> derive(shFX fx) {
    return std::static_pointer_cast<T>(fx);
  }
};

class FX {
public:
  FX();
  virtual ~FX() = default;

  virtual bool completed() noexcept { return finished; }

  bool match_name(const std::initializer_list<csv> names) const noexcept;
  bool match_name(csv n) const { return n == name(); }
  virtual csv name() const = 0;

  // workhorse of FX
  bool render(frame_t frame, desk::DataMsg &msg) noexcept;

  virtual void once() {} // subclasses should override once() to run setup code one time

protected:
  virtual void execute(Peaks &peaks) = 0;

protected:
  static Units units;
  bool finished = false;

private:
  bool called_once = false;

public:
  static constexpr csv module_id{"FX"};
};

} // namespace pierre
