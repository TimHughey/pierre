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
#include "base/dura_t.hpp"
#include "base/elapsed.hpp"
#include "base/input_info.hpp"
#include "base/types.hpp"

#include <concepts>
#include <fmt/format.h>
#include <utility>

namespace pierre {

template <typename T>
concept HasTimestamp = requires(T &f) {
  { f.ts() } noexcept;
};

template <typename T>
concept HasClockNow = requires(T &c) {
  { c.now() } noexcept;
};

class AnchorLast {
  friend struct fmt::formatter<AnchorLast>;

public:
  ClockID clock_id{0}; // senders network timeline id (aka clock id)
  uint32_t rtp_time{0};
  Nanos anchor_time{0};
  Nanos localized{0};
  Elapsed since_update;
  Nanos master_at{0};

  AnchorLast() = default;

  bool age_check(const auto age_min) const noexcept { return ready() && (since_update > age_min); }

  template <typename T>
    requires HasTimestamp<T>
  Nanos frame_local_time_diff(const T &f) const noexcept {
    return frame_to_local_time(f) - Nanos(clock_now::mono::ns());
  }

  Nanos frame_local_time_diff(ftime_t ts) const noexcept {
    return frame_to_local_time(ts) - Nanos(clock_now::mono::ns());
  }

  template <typename T>
    requires HasTimestamp<T>
  Nanos frame_to_local_time(const T &f) const noexcept {
    int32_t frame_diff = f.ts() - rtp_time;
    Nanos time_diff = Nanos((frame_diff * ipow10(9)) / InputInfo::rate);

    return localized + time_diff;
  }

  Nanos frame_to_local_time(ftime_t timestamp) const noexcept {
    int32_t frame_diff = timestamp - rtp_time;
    Nanos time_diff = Nanos((frame_diff * ipow10(9)) / InputInfo::rate);

    return localized + time_diff;
  }

  explicit operator bool() const noexcept { return ready(); }

  ftime_t local_to_frame_time(const Nanos local_time) const noexcept {
    Nanos time_diff = local_time - localized;
    Nanos frame_diff = time_diff * InputInfo::rate;

    return rtp_time + (frame_diff.count() / ipow10(9));
  }

  bool ready() const noexcept { return clock_id != 0; }
  void reset() noexcept { *this = AnchorLast(); }

  void update(const auto &ad, const auto &clock) noexcept {

    rtp_time = ad.rtp_time;
    anchor_time = ad.anchor_time;
    dura::apply_offset(localized, anchor_time, clock.rawOffset);
    since_update.reset();

    // only update master when AnchorLast isn't ready
    if (clock_id == 0x00) {
      master_at = clock.mastershipStartTime;
      clock_id = ad.clock_id; // denotes AnchorLast is ready
    }
  }

public:
  MOD_ID("frame.anc.last");
};

} // namespace pierre

/// @brief Custom formatter for ClockInfo
template <> struct fmt::formatter<pierre::AnchorLast> : formatter<std::string> {
  using AnchorLast = pierre::AnchorLast;

  // parse is inherited from formatter<string>.
  template <typename FormatContext> auto format(const AnchorLast &al, FormatContext &ctx) const {
    auto msg = fmt::format("clk_id={:x} rtp_time={:x} anc_time={:x}", //
                           al.clock_id, al.rtp_time, al.anchor_time);

    return formatter<std::string>::format(msg, ctx);
  }
};