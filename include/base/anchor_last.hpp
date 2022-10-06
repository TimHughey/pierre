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

#include "base/clock_info.hpp"
#include "base/elapsed.hpp"
#include "base/input_info.hpp"
#include "base/pet.hpp"
#include "base/typical.hpp"

#include <any>
#include <tuple>
#include <utility>

namespace pierre {

using frame_local_time_result_t = std::pair<bool, Nanos>;

class AnchorLast {
public:
  ClockID clock_id{0}; // senders network timeline id (aka clock id)
  uint32_t rtp_time{0};
  Nanos anchor_time{0};
  Nanos localized{0};
  Nanos valid;

  AnchorLast() = default;

  AnchorLast(ClockID id, uint32_t rtp_time, Nanos anchor_time, const ClockInfo &clock)
      : clock_id(id),                                                  //
        rtp_time(rtp_time),                                            //
        anchor_time(anchor_time),                                      //
        localized(pet::subtract_offset(anchor_time, clock.rawOffset)), //
        valid(clock.master_for())                                      //
  {}

  Nanos frame_local_time_diff(uint32_t timestamp) const {
    auto diff = Nanos::zero();
    auto now = pet::now_monotonic();

    auto frame_local_time = frame_to_local_time(timestamp);
    diff = frame_local_time - now;
    __LOGX(LCOL01 " localized={:^26} frame_local_time={:^26} now={:^26} diff={:^12}\n", module_id,
           "FRAME_DIFF", pet::humanize(localized), pet::humanize(frame_local_time),
           pet::humanize(now), pet::humanize(diff));

    // __LOG0(LCOL01 " diff={}\n", module_id, "FRAME_DIFF", pet::humanize(diff));

    return diff;
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

  Nanos frame_to_local_time(uint32_t timestamp) const {
    int32_t frame_diff = timestamp - rtp_time;
    Nanos time_diff = Nanos((frame_diff * pet::NS_FACTOR.count()) / InputInfo::rate);

    return localized + time_diff;
  }

  bool viable() const { return valid >= ClockInfo::AGE_MIN; }

public:
  static constexpr csv module_id{"ANCHOR_LAST"};
};

} // namespace pierre