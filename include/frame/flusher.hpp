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

#include "base/pet_types.hpp"
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

  bool acquire(Millis try_ms = 900ms) noexcept { return sema.try_acquire_for(try_ms); }

  /// @brief Determine if the frame should be kept
  /// @tparam T Any object that has seq_num and timestamp data members
  /// @param frame What to examine
  /// @return boolean indicating if frame meets flush criteria
  template <typename T>
    requires IsFrame<T>
  bool check(T &frame) noexcept {
    if (fi.inactive()) return false;

    auto flush{true};
    flush &= fi.all() || std::cmp_less_equal(frame.sn(), fi.until_seq);
    flush &= fi.all() || std::cmp_less_equal(frame.ts(), fi.until_ts);

    if (flush) ++fi.flushed;

    return flush;
  }

  /// @brief Reset count of frames flushed
  /// @return number of frames frames
  auto count() noexcept { return fi.flushed; }

  /// @brief Mark this flush as finished (active() == false)
  void done() noexcept;

  void release() noexcept { sema.release(); }

private:
  // order dependent
  FlushInfo fi;
  std::binary_semaphore sema;

public:
  MOD_ID("desk.flusher");
};

} // namespace pierre
