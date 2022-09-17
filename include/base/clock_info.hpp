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
#include "base/pet.hpp"
#include "base/typical.hpp"

namespace pierre {

using MasterIP = std::string;

struct ClockInfo {
  ClockID clock_id{0};          // current master clock
  MasterIP masterClockIp{0};    // IP of master clock
  uint64_t sampleTime{0};       // time when the offset was calculated
  uint64_t rawOffset{0};        // master clock time = sampleTime + rawOffset
  Nanos mastershipStartTime{0}; // when the master clock became master

  static constexpr Nanos AGE_MIN = 1500ms;
  static constexpr Nanos AGE_STABLE = 5s;
  static constexpr Nanos SAMPLE_AGE_MAX = 10s;

  bool is_minimum_age() const { return master_for_at_least(AGE_MIN); }
  bool is_stable() const { return master_for_at_least(AGE_STABLE); }

  Nanos master_for(Nanos ref = pet::now_monotonic()) const {
    return pet::not_zero(mastershipStartTime) ? pet::elapsed(mastershipStartTime, ref)
                                              : Nanos::zero();
  }

  bool master_for_at_least(const Nanos min, Nanos ref = pet::now_monotonic()) const {
    return (master_for(ref) >= min) ? true : false;
  }

  bool match_clock_id(const ClockID id) const { return id == clock_id; }

  bool ok() const {
    bool rc = clock_id && pet::not_zero(mastershipStartTime);

    if (!rc) { // no master clock or too young
      __LOG0(LCOL01 " no clock info, clock={:#02x} master_ship_time={}\n", module_id,
             csv("NOTICE"), clock_id, pet::not_zero(mastershipStartTime));
    }

    return rc;
  }

  Nanos sample_age(Nanos now = pet::now_monotonic()) const {
    return ok() ? pet::elapsed_as<Nanos>(sample_time(), now) : Nanos::zero();
  }

  Nanos sample_time() const { return Nanos(sampleTime); }

  bool sample_old(auto age_max = SAMPLE_AGE_MAX) const {
    if (auto age = sample_age(); age >= age_max) {
      return log_age_issue("SAMPLE OLD", age);
    }

    return false;
  }

  // misc debug
  const string inspect() const;

private:
  bool log_age_issue(csv msg, const Nanos &diff) const {
    __LOG0(LCOL01 " clock_id={:#x} sampleTime={} age={}\n", //
           module_id, msg, clock_id, pet::as<MillisFP>(sample_time()), pet::as_secs(diff));

    return true; // return false, final rc is inverted
  }

  void log_clock_time([[maybe_unused]] csv category, [[maybe_unused]] Nanos actual) const {

    __LOGX(LCOL01 " clock_id={:#x} now={:02.3}\n", //
           module_id, category, clock_id, pet::humanize(actual));
  }

  void log_clock_status() const {
    __LOGX(LCOL01 " clock_id={:#x} is_minimum_age={} is_stable={} master_for={}\n", //
           module_id, "STATUS", clock_id, is_stable(), is_minimum_age(),
           pet::humanize(master_for()));
  }

  void log_timeout() const {
    __LOGX(LCOL01 " waiting for clock_id={:#x} master_for={}\n", //
           module_id, "TIMEOUT", clock_id, pet::humanize(master_for()));
  }

  static constexpr csv module_id{"CLOCK_INFO"};
};

} // namespace pierre

;
