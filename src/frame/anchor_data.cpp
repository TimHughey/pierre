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

#include "anchor_data.hpp"
#include "base/input_info.hpp"
#include "base/logger.hpp"
#include "base/types.hpp"

#include <fmt/chrono.h>
#include <iterator>

namespace pierre {

void AnchorData::log_timing_change(const AnchorData &ad) const noexcept {
  INFO_AUTO_CAT("timing_change");

  string msg;
  auto w = std::back_inserter(msg);

  if (ad.clock_id == 0) {
    fmt::format_to(w, "NO CLOCK");

  } else {
    double time_diff = ad.anchor_time.count() - anchor_time.count();
    auto frame_diff = ad.rtp_time - rtp_time;

    double time_diff_in_frames = (1.0 * time_diff * InputInfo::rate) / ipow10(9);
    millis_fp frame_change{frame_diff - time_diff_in_frames};

    fmt::format_to(w, "clk_id={:#x} rtp_time={:02x} frame_adj={:0.2}", //
                   ad.clock_id, ad.rtp_time, frame_change);

    if (clock_id != ad.clock_id) fmt::format_to(w, " ** NEW **", ad.clock_id);
  }

  INFO_AUTO("{}", msg);
}

} // namespace pierre
