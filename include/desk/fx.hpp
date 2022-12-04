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

#include <atomic>
#include <initializer_list>
#include <memory>

namespace pierre {

class FX {
public:
  FX();

  virtual bool completed() noexcept { return finished.load(); }

  bool match_name(const std::initializer_list<csv> names) const noexcept;
  bool match_name(csv n) const { return n == name(); }
  virtual csv name() const = 0;

  /// @brief Called for every frame to render the embedded peaks
  /// @param frame Audio frame containing peaks for rendering
  /// @param msg Data message to populate (sent to remote render controller)
  /// @return boolean indicating if FX is complete (safe to switch to another FX)
  bool render(frame_t frame, desk::DataMsg &msg) noexcept;

  /// @brief FX subclasses should override to run setup code once per creation
  virtual void once() {}

  virtual std::shared_ptr<FX> ptr_base() noexcept = 0;

protected:
  virtual void execute(Peaks &peaks) = 0;

  void set_finished(bool yes_or_no = true) noexcept { finished.store(yes_or_no); }

protected:
  static Units units;
  std::atomic_bool finished;

private:
  bool called_once{false};

public:
  static constexpr csv module_id{"FX"};
};

} // namespace pierre
