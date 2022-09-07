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

#include "base/pet.hpp"
#include "base/typical.hpp"

namespace pierre {

using MasterIP = std::string;

struct ClockInfo {
  ClockID clock_id{0};          // current master clock
  MasterIP masterClockIp{0};    // IP of master clock
  Nanos sampleTime{0};          // time when the offset was calculated
  Nanos rawOffset{0};           // master clock time = sampleTime + rawOffset
  Nanos mastershipStartTime{0}; // when the master clock became master

  static constexpr Nanos AGE_MAX{10s};
  static constexpr Nanos AGE_MIN{1500ms};

  auto operator()() const {
    log_raw_offset("OPERATOR");

    return rawOffset;
  }

  Nanos masterFor(Nanos now = pet::now_nanos()) const { return now - mastershipStartTime; }

  bool ok() const {
    bool rc = clock_id != 0;

    if (!rc) { // no master clock
      __LOG0(LCOL01 " no clock info\n", module_id, csv("WARN"));
    }

    return rc;
  }

  Nanos sampleAge(Nanos now = pet::now_nanos()) const {
    return ok() ? pet::elapsed_abs_ns(sampleTime, now) : Nanos::zero();
  }

  Nanos local_network_time(const Nanos network_time) const {
    log_raw_offset("LOCAL_NET");

    return network_time - rawOffset;
  }

  bool tooOld(auto age_max = AGE_MAX) const {
    if (auto age = sampleAge(); age >= age_max) {
      return log_age_issue("TOO OLD", age);
    }

    return false;
  }

  // misc debug
  const string inspect() const;

private:
  bool log_age_issue(csv msg, const Nanos &diff) const {
    __LOG0(LCOL01 " clock_id={:#x} sampleTime={} age={}\n", //
           module_id, msg, clock_id, sampleTime, pet::as_secs(diff));

    return true; // return false, final rc is inverted
  }

  void log_raw_offset([[maybe_unused]] csv category) const {
    const auto offset_secs = pet::as<Seconds, Nanos>(rawOffset);
    const auto offset = rawOffset - offset_secs;

    __LOGX(LCOL01 " clock_id={:#x} offset={:02.3}\n", //
           module_id, category, clock_id, pet::as_millis_fp(offset));
  }

  static constexpr csv module_id{"CLOCK_INFO"};
};

} // namespace pierre
