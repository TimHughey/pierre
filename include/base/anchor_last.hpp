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

#include "base/anchor_data.hpp"
#include "base/clock_info.hpp"
#include "base/elapsed.hpp"
#include "base/input_info.hpp"
#include "base/pet.hpp"
#include "base/types.hpp"

#include <tuple>
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

  bool age_check(const auto age_min) const noexcept {
    return ready() && (since_update() > age_min);
  }

  Nanos frame_local_time_diff(uint32_t timestamp) const {
    auto diff = Nanos::zero();
    auto now = pet::now_monotonic();

    auto frame_local_time = frame_to_local_time(timestamp);
    return frame_local_time - now;
  }

  /* int frame_to_ptp_local_time(uint32_t timestamp, uint64_t *time, rtsp_conn_info *conn) {
    int result = -1;
    uint32_t anchor_rtptime = 0;
    uint64_t anchor_local_time = 0;
    if (get_ptp_anchor_local_time_info(conn, &anchor_rtptime, &anchor_local_time) == clock_ok) {
      int32_t frame_difference = timestamp - anchor_rtptime;
      int64_t time_difference = frame_difference;
      time_difference = time_difference * 1000000000;
      if (conn->input_rate == 0)
        die("conn->input_rate is zero!");
      time_difference = time_difference / conn->input_rate;
      uint64_t ltime = anchor_local_time + time_difference;
      *time = ltime;
      result = 0;
    } else {
      debug(3, "frame_to_local_time can't get anchor local time information");
    }
    return result;
  } */

  Nanos frame_to_local_time(timestamp_t timestamp) const noexcept {
    int32_t frame_diff = timestamp - rtp_time;
    Nanos time_diff = Nanos((frame_diff * pet::NS_FACTOR.count()) / InputInfo::rate);

    return localized + time_diff;
  }

  /* int local_ptp_time_to_frame(uint64_t time, uint32_t *frame, rtsp_conn_info *conn) {
 int result = -1;
 uint32_t anchor_rtptime = 0;
 uint64_t anchor_local_time = 0;
 if (get_ptp_anchor_local_time_info(conn, &anchor_rtptime, &anchor_local_time) == clock_ok) {
   int64_t time_difference = time - anchor_local_time;
   int64_t frame_difference = time_difference;
   frame_difference = frame_difference * conn->input_rate; // but this is by 10^9
   frame_difference = frame_difference / 1000000000;
   int32_t fd32 = frame_difference;
   uint32_t lframe = anchor_rtptime + fd32;
   *frame = lframe;
   result = 0;
 } else {
   debug(3, "local_time_to_frame can't get anchor local time information");
 }
 return result;
} */

  timestamp_t local_to_frame_time(const Nanos local_time = pet::now_monotonic()) const noexcept {
    Nanos time_diff = local_time - localized;
    Nanos frame_diff = time_diff * InputInfo::rate;

    return rtp_time + (frame_diff.count() / pet::NS_FACTOR.count());
  }

  bool ready() const noexcept { return clock_id != 0; }
  void reset() noexcept { *this = AnchorLast(); }

  void update(const AnchorData &ad, const ClockInfo &clock) noexcept {

    rtp_time = ad.rtp_time;
    anchor_time = ad.anchor_time;
    localized = pet::subtract_offset(anchor_time, clock.rawOffset);
    since_update.reset();

    if (clock_id == 0x00) { // only update master when AnchorLast isn't ready
      master_at = clock.mastershipStartTime;
      clock_id = ad.clock_id; // denotes AnchorLast is ready
    }
  }

public:
  static constexpr csv module_id{"ANCHOR_LAST"};
};

} // namespace pierre