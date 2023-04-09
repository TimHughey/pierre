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
#include "base/types.hpp"
#include "base/uint8v.hpp"

#include <future>
#include <memory>

namespace pierre {

// forward decl
class MasterClock;

struct ClockPort {
  string id;
  Port port;
};

using ClockPorts = std::vector<ClockPort>;

struct PeerInfo {
  string id;
  uint8v addresses;
  ClockPorts clock_ports;
  int device_type{0};
  ClockID clock_id{0};
  bool port_matching_override{false};
};

using Peers = std::vector<string>;
using PeerList = std::vector<PeerInfo>;
using MasterIP = std::string;

struct ClockInfo {
  ClockID clock_id{0};          // current master clock
  MasterIP masterClockIp{0};    // IP of master clock
  uint64_t sampleTime{0};       // time when the offset was calculated
  uint64_t rawOffset{0};        // master clock time = sampleTime + rawOffset
  Nanos mastershipStartTime{0}; // when the master clock became master
  enum { EMPTY, READ, OK, STABLE } status{EMPTY};
  Elapsed sample_age;

  static constexpr Nanos AGE_STABLE{5s};
  static constexpr Nanos INFO_MAX_WAIT{133ms}; // typical sample refresh ~122ms
  static constexpr Nanos SAMPLE_AGE_MAX{10s};

  ClockInfo() = default;

  ClockInfo(ClockID clock_id, const MasterIP &master_clock_ip, uint64_t sample_time,
            uint64_t raw_offset, const Nanos &master_start_time) noexcept
      : clock_id(clock_id), masterClockIp(master_clock_ip), sampleTime(sample_time),
        rawOffset(raw_offset), mastershipStartTime(master_start_time), status(READ) {
    if (is_stable()) {
      status = ClockInfo::STABLE;
    } else if (ok()) {
      status = ClockInfo::OK;
    }
  }

  bool is_stable() const { return master_for_at_least(AGE_STABLE); }

  Nanos master_for() const noexcept;

  bool master_for_at_least(const Nanos min) const noexcept {
    return (master_for() >= min) ? true : false;
  }

  bool match_clock_id(const ClockID id) const { return id == clock_id; }

  bool ok() const noexcept;

  Nanos sample_time() const { return Nanos(sampleTime); }

  bool old(auto age_max = SAMPLE_AGE_MAX) const noexcept { return sample_age >= age_max; }

  // misc debug
  const string inspect() const;

private:
  static constexpr csv module_id{"frame.clock_info"};
};

typedef const ClockInfo ClockInfoConst;

} // namespace pierre
