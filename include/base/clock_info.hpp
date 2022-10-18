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
#include "base/logger.hpp"
#include "base/pet.hpp"
#include "base/types.hpp"

#include <future>
#include <memory>

namespace pierre {

using MasterIP = std::string;

struct ClockInfo;
using clock_info_future = std::future<ClockInfo>;
using clock_info_promise_ptr = std::shared_ptr<std::promise<ClockInfo>>;

struct ClockInfo {
  ClockID clock_id{0};          // current master clock
  MasterIP masterClockIp{0};    // IP of master clock
  uint64_t sampleTime{0};       // time when the offset was calculated
  uint64_t rawOffset{0};        // master clock time = sampleTime + rawOffset
  Nanos mastershipStartTime{0}; // when the master clock became master
  enum { EMPTY, READ, MIN_AGE, STABLE } status{EMPTY};

  static constexpr Nanos AGE_MIN = 1500ms;
  static constexpr Nanos AGE_STABLE = 5s;
  static constexpr Nanos SAMPLE_AGE_MAX = 10s;

  ClockInfo() = default;

  ClockInfo(ClockID clock_id,                        //
            const MasterIP &master_clock_ip,         //
            uint64_t sample_time,                    //
            uint64_t raw_offset,                     //
            const Nanos &master_start_time) noexcept //
      : clock_id(clock_id),                          //
        masterClockIp(master_clock_ip),              //
        sampleTime(sample_time),                     //
        rawOffset(raw_offset),                       //
        mastershipStartTime(master_start_time),      //
        status(READ)                                 //
  {
    if (is_stable()) {
      status = ClockInfo::STABLE;
    } else if (is_minimum_age()) {
      status = ClockInfo::MIN_AGE;
    }
  }

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
      INFO(module_id, "NOTiCE", "no clock info, clock={:#02x} master_ship_time={}\n", clock_id,
           pet::not_zero(mastershipStartTime));
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

  bool useable() const { return ((status == STABLE) || (status == MIN_AGE)); }
  Nanos until_min_age() const {
    Nanos until = ClockInfo::AGE_MIN - master_for();

    return (until > Nanos::zero()) ? until : Nanos::zero();
  }

  // misc debug
  const string inspect() const;

private:
  bool log_age_issue(csv msg, const Nanos &diff) const {
    INFO(module_id, msg, "clock_id={:#x} sampleTime={} age={}\n", //
         clock_id, pet::as<MillisFP>(sample_time()), pet::as_secs(diff));

    return true; // return false, final rc is inverted
  }

  void log_clock_time([[maybe_unused]] csv category, [[maybe_unused]] Nanos actual) const {
    INFOX(module_id, category, "clock_id={:#x} now={:02.3}\n", //
          clock_id, pet::humanize(actual));
  }

  void log_clock_status() const {
    INFOX(module_id, "STATUS", "clock_id={:#x} is_minimum_age={} is_stable={} master_for={}\n", //
          clock_id, is_stable(), is_minimum_age(), pet::humanize(master_for()));
  }

  void log_timeout() const {
    INFOX(module_id, "TIMEOUT", "waiting for clock_id={:#x} master_for={}\n", //
          clock_id, pet::humanize(master_for()));
  }

  static constexpr csv module_id{"CLOCK_INFO"};
};

} // namespace pierre

;
