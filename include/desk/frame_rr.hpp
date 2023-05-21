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

struct frame_rr {
  frame_rr(Frame &&f) noexcept : f(std::move(f)) {}

  Frame f;
  bool stop{false};

  bool abort() const noexcept { return stop; }
  bool ok() const noexcept { return !abort(); }

  void finish() noexcept {
    f.record_state();
    f.record_sync_wait();

    if (f.ready()) {
      f.mark_rendered();
    }
  }

  void operator()(Frame &&f) noexcept { f = std::move(f); }
  Frame &operator()() noexcept { return f; }
};

} // namespace desk
} // namespace pierre