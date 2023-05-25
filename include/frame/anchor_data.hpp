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
#include "base/elapsed.hpp"
#include "base/qpow10.hpp"
#include "base/types.hpp"
#include "frame/clock_info.hpp"

#include <compare>
#include <fmt/format.h>

namespace pierre {

template <typename T>
concept HasClockId = requires(T &t) {
  { t.clock_id } -> std::convertible_to<ClockID>;
};

struct AnchorData {
  //  Requirements and implementation notes when using PTP timing:
  //    * The local (monotonic system up)time in nanos (arbitrary reference)
  //    * The remote (monotonic system up)time in nanos (arbitrary reference)
  //    * (symmetric) link delay
  //      1. calculate link delay (PTP)
  //      2. get local time (PTP)
  //      3. calculate remote time (nanos) wrt local time (nanos) w/PTP. Now
  //         we know how remote timestamps align to local ones. Now these
  //         network times are meaningful.
  //      4. determine how many nanos elapsed since anchorTime msg egress.
  //         Note: remote monotonic nanos for iPhones stops when they sleep, though
  //         not when casting media.

  ClockID clock_id{0}; // senders network timeline id (aka clock id)
  uint64_t flags{0};
  uint32_t rtp_time{0};
  Nanos anchor_time{0};
  Nanos master_for{0};

  // for last
  Nanos localized{0};
  Elapsed localized_elapsed;
  Nanos valid_at{0};

  AnchorData(ClockID clock_id,  // network timeline id (aka senders clock id)
             uint64_t secs,     // anchor time seconds (arbitrary reference)
             uint64_t fracs,    // anchor time fractional nanoseconds
             uint64_t rtp_time, // rtp time (as defined by sender)
             uint64_t flags)    // unknown and unused at present
      noexcept
      : clock_id(clock_id),                            // directly save the clock id
        flags(flags),                                  // directly save flags
        rtp_time(static_cast<uint32_t>(rtp_time)),     // rtp time is 32 bits
        anchor_time(Seconds(secs) + nano_fracs(fracs)) // combine network time secs and fracs
  {}

  AnchorData() = default;

  bool maybe_unstable(const AnchorData &ad) const noexcept {
    return (clock_id == ad.rtp_time)
               ? ((rtp_time != ad.rtp_time) || (anchor_time != ad.anchor_time))
               : false;
  }

  void log_timing_change(const AnchorData &ad) const noexcept;

  bool master_for_at_least(const auto master_min) const noexcept {
    return (master_for == 0ns) ? false : master_for >= master_min;
  }

  template <typename T>
    requires HasClockId<T>
  bool operator==(const T &lhs) const noexcept {
    return clock_id == lhs.clock_id;
  }

private:
  static Nanos nano_fracs(uint64_t fracs) noexcept {
    return Nanos(((fracs >> 32) * qpow10(9)) >> 32);
  }

public:
  MOD_ID("frame.anc.data");
};

} // namespace pierre

/// @brief Custom formatter for AnchorData
template <> struct fmt::formatter<pierre::AnchorData> : formatter<std::string> {
  using AnchorData = pierre::AnchorData;

  // parse is inherited from formatter<string>.
  template <typename FormatContext> auto format(AnchorData &ad, FormatContext &ctx) const {
    auto msg = fmt::format("clk_id={:x} rtp_time={:08x}", ad.clock_id, ad.rtp_time);

    return formatter<std::string>::format(msg, ctx);
  }
};
