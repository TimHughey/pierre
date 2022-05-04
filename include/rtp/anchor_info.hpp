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

#include <cstdint>
#include <fmt/format.h>
#include <source_location>

#include "nptp/clock_info.hpp"
#include "nptp/nptp.hpp"

namespace pierre {
namespace rtp {

struct AnchorData {
  uint64_t rate = 0;
  uint64_t timelineID = 0; // aka clock id
  uint64_t secs = 0;
  uint64_t frac = 0;
  uint64_t flags = 0;
  uint64_t rtpTime = 0;
  uint64_t networkTime = 0; // from set anchor packet
  uint64_t anchorTime = 0;
  uint64_t anchorRtpTime = 0;
  sNptp nptp = nullptr; // dependency injection
};

class AnchorInfo {
private:
  using src_loc = std::source_location;

public:
  AnchorInfo() = default;

  // copy/move constructors (from AnchorData)
  AnchorInfo(const AnchorData &ad);
  AnchorInfo(const AnchorData &&ad);

  // copy/move constructors (from AnchorInfo)
  AnchorInfo(const AnchorInfo &ai);
  AnchorInfo(AnchorInfo &&ai);

  // copy/move assignment (from AnchorData)
  AnchorInfo &operator=(const AnchorData &ad);
  AnchorInfo &operator=(AnchorData &&ad);

  // copy/move assignment (from AnchorInfo)
  AnchorInfo &operator=(const AnchorInfo &ai);
  AnchorInfo &operator=(AnchorInfo &&ai);

  void dump(const src_loc loc = src_loc::current());

public:
  AnchorData data;
  AnchorData actual;

  nptp::ClockInfo clock_info;

  sNptp nptp;

  bool _play_enabled = false;

private:
  void __init();

  void calcNetTime();
  void chooseAnchorClock();

  void infoNewClock();
  void warnFrequentChanges();

  static const char *fnName(const std::source_location loc = std::source_location::current()) {
    return loc.function_name();
  }

private:
  uint64_t anchor_clock = 0;
  uint64_t anchor_rptime = 0;
  uint64_t anchor_time = 0;

  uint64_t anchor_clock_new_ns = 0;

  bool _debug_ = false;
};

} // namespace rtp
} // namespace pierre