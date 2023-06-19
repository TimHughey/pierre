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
#include "frame/frame.hpp"

namespace pierre {
namespace desk {

/// @brief Desk render loop state
struct frame_rr {
  /// @brief Construct from a Frame
  /// @param f Frame (is moved)
  frame_rr(Frame &&f) noexcept : f(std::move(f)) {}

  Frame f;
  bool stop{false};

  /// @brief Determine if the render loop should stop
  /// @return boolean
  bool abort() const noexcept { return stop; }

  /// @brief Determine if the render loop should continue
  /// @return
  bool ok() const noexcept { return !abort(); }

  /// @brief Rendering is complete, do housekeeping
  void finish() noexcept {
    f.record_state();
    f.record_sync_wait();

    if (f.ready()) {
      f.mark_rendered();
    }
  }

  /// @brief Replace the Frame being rendered
  /// @param f Frame (is moved)
  void operator()(Frame &&f) noexcept { f = std::move(f); }

  /// @brief Get reference to the Frame being rendered
  /// @return Frame reference
  Frame &operator()() noexcept { return f; }

  /// @brief The sync wait of the frame
  /// @return Nanos
  Nanos sync_wait() noexcept { return f.sync_wait(); }
};

} // namespace desk
} // namespace pierre