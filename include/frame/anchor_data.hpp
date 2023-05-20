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

#include "base/elapsed.hpp"
#include "base/pet_types.hpp"
#include "base/qpow10.hpp"
#include "base/types.hpp"
#include "frame/clock_info.hpp"

#include <fmt/format.h>

namespace pierre {

struct AnchorData {
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

  bool diff(const AnchorData &ad) const noexcept {
    return (clock_id != ad.clock_id) || (rtp_time != ad.rtp_time) ||
           (anchor_time != ad.anchor_time);
  }

  bool empty() const { return !clock_id; }

  bool master_for_at_least(const auto master_min) const noexcept {
    return (master_for == Nanos::zero()) ? false : master_for >= master_min;
  }

  bool match_clock_id(const auto &to_match) const noexcept { return clock_id == to_match.clock_id; }

  bool match_details(const AnchorData &ad) const noexcept {
    return (anchor_time == ad.anchor_time) || (rtp_time == ad.rtp_time);
  }

private:
  static Nanos nano_fracs(uint64_t fracs) noexcept {
    return Nanos(((fracs >> 32) * qpow10(9)) >> 32);
  }

public:
  // misc debug
  string inspect() const;

  // void log_new(const AnchorData &old, const ClockInfo &clock) const;
  // void log_new_master_if_needed(bool &data_new) const;
  void log_timing_change(const AnchorData &ad) const noexcept;

public:
  MOD_ID("frame.anc.data");
};

} // namespace pierre

/// @brief Custom formatter for AnchorData
template <> struct fmt::formatter<pierre::AnchorData> : formatter<std::string> {
  using AnchorData = pierre::AnchorData;

  // parse is inherited from formatter<string>.
  template <typename FormatContext> auto format(AnchorData &ad, FormatContext &ctx) const {
    std::string msg;
    auto w = std::back_inserter(msg);

    fmt::format_to(w, "ANC_DATA clk_id=0x{:02x} ", ad.clock_id);
    fmt::format_to(w, "rtp_time={:08x} ", ad.rtp_time);

    return formatter<std::string>::format(msg, ctx);
  }
};
