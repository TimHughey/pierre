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

#include "rtsp/reply/anchor.hpp"

namespace pierre {
namespace rtsp {

Anchor::Anchor(const Reply::Opts &opts) : Reply(opts), Aplist(requestContent()) {
  // maybe more
}

bool Anchor::populate() {
  // more later

  return true;
}

void Anchor::calcRtpTime() {
  constexpr auto NetworkTimeSecs = "networkTimeSecs";
  constexpr auto NetworkTimeFrac = "networkTimeFrac";
  constexpr auto NetworkTimeTimelineID = "networkTimeTimelineID";
  constexpr auto RtpTime = "rtpTime";

  try {
    uint64_t net_time_secs = dictGetUint(1, NetworkTimeSecs);
    uint64_t net_time_frac = dictGetUint(1, NetworkTimeFrac);
    [[maybe_unused]] uint64_t rtp_time = dictGetUint(1, RtpTime);
    [[maybe_unused]] uint64_t nid = dictGetUint(1, NetworkTimeTimelineID);

    // it looks like the networkTimeFrac is a fraction where the msb is work
    // 1/2, the next 1/4 and so on now, convert the network time and fraction
    // into nanoseconds
    net_time_frac = net_time_frac >> 32; // reduce precision to about 1/4 nanosecond
    net_time_frac = net_time_frac * 1000000000;
    net_time_frac = net_time_frac >> 32; // we should now be left with the ns

    net_time_frac = net_time_secs * 1000000000; // turn the whole seconds into ns
    [[maybe_unused]] uint64_t anchor_ns = net_time_secs + net_time_frac;

  } catch (const std::out_of_range &) {
    fmt::print("{] did not find expected network time keys", fnName());
  }
}

} // namespace rtsp
} // namespace pierre