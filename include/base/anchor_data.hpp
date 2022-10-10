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

struct AnchorData {
  ClockID clock_id{0}; // senders network timeline id (aka clock id)
  uint64_t flags{0};
  uint32_t rtp_time{0};
  Nanos anchor_time{0};
  Nanos master_for{0};

  // for last
  Nanos localized{0};
  Elapsed localized_elapsed;
  bool valid = false;
  Nanos valid_at{0};

  AnchorData(ClockID clock_id,  // network timeline id (aka senders clock id)
             uint64_t secs,     // anchor time seconds (arbitrary reference)
             uint64_t fracs,    // anchor time fractional nanoseconds
             uint64_t rtp_time, // rtp time (as defined by sender)
             uint64_t flags)    // unknown and unused at present
      noexcept
      : clock_id(clock_id),                             // directly save the clock id
        flags(flags),                                   // directly save flags
        rtp_time(static_cast<uint32_t>(rtp_time)),      // rtp time is 32 bits
        anchor_time(Seconds(secs) + Nanos(fracs >> 32)) // combine secs and fracs
  {
    __LOGX(LCOL01 " anchor_time={}\n", module_id, "DEBUG", pet::humanize(anchor_time));
  }

  AnchorData() = default;

  void change_clock_id(const ClockInfo &clock) { clock_id = clock.clock_id; }

  bool details_changed(const AnchorData &ad) const {
    return (clock_id != ad.clock_id) || (rtp_time != ad.rtp_time) ||
           (anchor_time != ad.anchor_time);
  }

  bool empty() const { return !clock_id; }

  bool master_for_at_least(const auto master_min) const {
    return (master_for == Nanos::zero()) ? false : master_for >= master_min;
  }

  bool match_clock_id(const AnchorData &ad) const { return clock_id == ad.clock_id; }
  bool match_clock_id(const ClockInfo &ci) const { return clock_id == ci.clock_id; }

  bool match_details(const AnchorData &ad) const {
    return (anchor_time == ad.anchor_time) || (rtp_time == ad.rtp_time);
  }

  bool not_viable() const { return !viable(); }

  friend bool operator==(const AnchorData &lhs, const AnchorData &rhs) {
    return std::tie(lhs.clock_id, lhs.rtp_time, lhs.anchor_time) ==
           std::tie(rhs.clock_id, rhs.rtp_time, rhs.anchor_time);
  }

  friend bool operator!=(const AnchorData &lhs, const AnchorData &rhs) {
    return std::tie(lhs.clock_id, lhs.rtp_time, lhs.anchor_time) !=
           std::tie(rhs.clock_id, rhs.rtp_time, rhs.anchor_time);
  }

  void reset() { *this = AnchorData(); }

  void set_master_for(const ClockInfo &clock) {
    if (match_clock_id(clock)) {
      master_for = clock.master_for();
    }
  }

  bool viable(const ClockInfo &clock) {
    set_master_for(clock);

    return viable();
  }

  bool viable() const {
    return (master_for == Nanos::zero()) ? false : master_for >= ClockInfo::AGE_STABLE;
  }

  static AnchorData any_cast(std::any &data) {
    return data.has_value() ? std::any_cast<AnchorData>(data) : AnchorData();
  }

private:
  Nanos clock_wait_time() const {
    return viable() ? Nanos::zero() : pet::diff_abs(master_for, ClockInfo::AGE_STABLE);
  }

public:
  // misc debug
  string inspect() const;

  void log_new(const AnchorData &old, const ClockInfo &clock) const;
  void log_new_master_if_needed(bool &data_new) const;

  static constexpr csv moduleID() { return module_id; }

private:
  static constexpr csv module_id{"ANCHOR_DATA"};
};

} // namespace pierre
