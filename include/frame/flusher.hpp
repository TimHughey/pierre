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

#include "base/dura_t.hpp"
#include "base/types.hpp"
#include "frame/flush_info.hpp"

#include <fmt/format.h>
#include <functional>
#include <semaphore>
#include <set>
#include <utility>

namespace pierre {

class Flusher {
  //// NOTE:
  ////
  //// UNSURE IF THE FOLLOWING IS CURRENT OR LEGACY
  //// a flush with from[seq|ts] will not be followed by a setanchor (i.e. render)
  //// if it's a flush that will be followed by a set_anchor then stop render now.
  ////

  friend struct fmt::formatter<Flusher>;

public:
  Flusher() noexcept : fi(), sema(1) {}
  ~Flusher() noexcept {}

  /// @brief Replace this with a newly created flush request
  ///        flush specifics are copied and kind is set (All, Normal)
  /// @param fi flush details
  void accept(FlushInfo &&fi) noexcept;

  bool acquire(Micros try_us = 500us) noexcept { return sema.try_acquire_for(try_us); }

  /// @brief Determine if the frame should be kept
  /// @tparam T Any object that has seq_num and timestamp data members
  /// @param frame What to examine
  /// @return boolean indicating if frame meets flush criteria
  template <typename T>
    requires IsFrame<T>
  bool check(T &f) noexcept {

    if (fi.inactive()) return false;

    bool rc = fi.all() || ((f.ts() <= fi.until_ts) && (f.sn() <= fi.until_seq));

    if (rc) {
      ++fi.flushed;
      f.flush();
    }

    return rc;
  }

  /// @brief Reset count of frames flushed
  /// @return number of frames frames
  auto count() noexcept { return fi.flushed; }

  /// @brief Mark this flush as finished (active() == false)
  void done() noexcept;

  auto release() noexcept {
    auto flushed = fi.flushed;

    sema.release();

    return flushed;
  }

private:
  // order dependent
  FlushInfo fi;
  std::binary_semaphore sema;

public:
  MOD_ID("desk.flusher");
};

} // namespace pierre
