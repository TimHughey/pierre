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
#include "base/dura.hpp"
#include "base/elapsed.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"

#include <fmt/chrono.h>
#include <fmt/format.h>

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

using MasterIP = std::string;

struct ClockInfo {
  ClockID clock_id{0};          // current master clock
  MasterIP masterClockIp{0};    // IP of master clock
  uint64_t sampleTime{0};       // time when the offset was calculated
  uint64_t rawOffset{0};        // master clock time = sampleTime + rawOffset
  Nanos mastershipStartTime{0}; // when the master clock became master
  Elapsed sample_age;
  int64_t now_ns{clock_mono_ns()};

  static constexpr Nanos AGE_STABLE{5s};
  static constexpr Nanos SAMPLE_AGE_MAX{133ms};

  ClockInfo() = default;

  ClockInfo(ClockID clock_id, const MasterIP &master_clock_ip, uint64_t sample_time,
            uint64_t raw_offset, const Nanos &master_start_time) noexcept
      : clock_id(clock_id), masterClockIp(master_clock_ip), sampleTime(sample_time),
        rawOffset(raw_offset), mastershipStartTime(master_start_time) {}

  ClockInfo(ClockInfo &&) = default;

  ~ClockInfo() noexcept {}

  bool is_stable() const { return master_for_at_least(AGE_STABLE); }

  Nanos master_for() const noexcept {
    auto raw_ns = clock_now::mono::ns() - mastershipStartTime.count();

    return (mastershipStartTime > 0ns) ? Nanos(raw_ns) : 0ns;
  }

  bool master_for_at_least(const Nanos min) const noexcept {
    return (master_for() >= min) ? true : false;
  }

  bool match_clock_id(const ClockID id) const { return id == clock_id; }

  bool ok() const noexcept { return clock_id && (mastershipStartTime > 0ns); }

  explicit operator bool() const noexcept { return ok(); }

  ClockInfo &operator=(ClockInfo &&) = default;
  bool operator!() const noexcept { return clock_id == 0; }

  int64_t now() const noexcept { return now_ns; }

  Nanos sample_time() const { return Nanos(sampleTime); }

  bool old() const noexcept { return sample_age >= SAMPLE_AGE_MAX; }

public:
  MOD_ID("frame.clock_info");
};

} // namespace pierre

/// @brief Custom formatter for ClockInfo
template <> struct fmt::formatter<pierre::ClockInfo> : formatter<std::string> {
  using ClockInfo = pierre::ClockInfo;
  using dura = pierre::dura;

  // parse is inherited from formatter<string>.
  template <typename FormatContext> auto format(const ClockInfo &ci, FormatContext &ctx) const {
    auto msg =
        fmt::format("clk_id={:x} raw={:x} samp_time={:x} master_for={} {}", ci.clock_id,
                    ci.rawOffset, ci.sampleTime, dura::humanize(ci.master_for()), ci.masterClockIp);

    return formatter<std::string>::format(msg, ctx);
  }
};
