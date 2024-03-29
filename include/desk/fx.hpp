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
#include "desk/dmx_data_msg.hpp"
#include "desk/units.hpp"
#include "frame/frame.hpp"
#include "fx/names.hpp"

#include <atomic>
#include <initializer_list>
#include <memory>

namespace pierre {

class FX {
public:
  /// @brief Construct the base FX via the subclassed type
  FX() noexcept;
  virtual ~FX() { cancel(); };

  /// @brief Cancel any pending io_ctx actions
  virtual void cancel() noexcept {}

  /// @brief Is the FX complete as determined by the subclass
  /// @return boolean indicating the FX is complete
  virtual bool completed() noexcept { return finished.load(); }

  /// @brief Match the FX to any of the names
  /// @param names List of names to match
  /// @return boolean indicating if any name matched
  bool match_name(const std::initializer_list<csv> names) const noexcept;

  /// @brief Match the FX to a single name
  /// @param n Name to match
  /// @return boolean indicating if the single name matched
  bool match_name(csv n) const { return n == name(); }

  /// @brief The FX name
  /// @return contant string view of the name
  virtual csv name() const = 0;

  /// @brief FX subclasses override to execute when FX called for the first time
  virtual void once() {}

  /// @brief Called for every frame to render the embedded peaks
  /// @param frame Audio frame containing peaks for rendering
  /// @param msg Data message to populate (sent to remote render controller)
  /// @return boolean indicating if FX is complete (safe to switch to another FX)
  bool render(frame_t frame, DmxDataMsg &msg) noexcept;

  /// @brief The next FX suggested by external configuration file
  /// @return name, as a string, of the suggested FX
  const string &suggested_fx_next() const noexcept { return next_fx; }

  /// @brief Will this FX render the audio peaks
  /// @return boolean, caller can use this flag to determine if upstream work is required

protected:
  /// @brief Execute the FX subclass for audio peaks for a single frame
  /// @param peaks The audio peaks to use for FX execution
  virtual void execute(Peaks &peaks) noexcept { peaks.noop(); };

  /// @brief Set the FX as complete
  /// @param is_finished Boolean indicating the finished status of the FX
  void set_finished(bool is_finished = true) noexcept { finished.store(is_finished); }

protected:
  /// @brief Order dependent class member data
  static Units units;
  std::atomic_bool finished;

  bool should_render{true};
  string next_fx;

private:
  bool called_once{false};

public:
  static constexpr csv module_id{"fx"};
};

} // namespace pierre
