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

#include "base/clock_now.hpp"
#include "base/elapsed.hpp"
#include "base/input_info.hpp"
#include "base/pet_types.hpp"
#include "base/types.hpp"
#include "frame/anchor_data.hpp"
#include "frame/clock_info.hpp"

#include <utility>

namespace pierre {

class AnchorLast {
public:
  ClockID clock_id{0}; // senders network timeline id (aka clock id)
  uint32_t rtp_time{0};
  Nanos anchor_time{0};
  Nanos localized{0};
  Elapsed since_update;
  Nanos master_at{0};

  AnchorLast() = default;

  bool age_check(const auto age_min) const noexcept { return ready() && (since_update > age_min); }

  Nanos frame_local_time_diff(uint32_t timestamp) const noexcept {
    return frame_to_local_time(timestamp) - Nanos{clock_now::mono::ns()};
  }

  Nanos frame_to_local_time(timestamp_t timestamp) const noexcept {
    int32_t frame_diff = timestamp - rtp_time;
    Nanos time_diff = Nanos((frame_diff * qpow10(9)) / InputInfo::rate);

    return localized + time_diff;
  }

  timestamp_t local_to_frame_time(const Nanos local_time = Nanos{
                                      clock_now::mono::ns()}) const noexcept {
    Nanos time_diff = local_time - localized;
    Nanos frame_diff = time_diff * InputInfo::rate;

    return rtp_time + (frame_diff.count() / qpow10(9));
  }

  bool ready() const noexcept { return clock_id != 0; }
  void reset() noexcept { *this = AnchorLast(); }

  void update(const AnchorData &ad, const ClockInfo &clock) noexcept;

public:
  static constexpr csv module_id{"anchor.last"};
};

} // namespace pierre