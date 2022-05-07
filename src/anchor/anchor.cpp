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

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fmt/format.h>
#include <memory>
#include <source_location>

#include "anchor/anchor.hpp"
#include "anchor/clock.hpp"
#include "core/host.hpp"
#include "core/input_info.hpp"

namespace pierre {

using namespace std::chrono_literals;
using Nanos = std::chrono::nanoseconds;

shAnchor __instance__; // definition of shared_ptr for Clock

Anchor::Anchor(shHost host) : clock(anchor::Clock(host)) {}
Anchor::~Anchor() {
  constexpr auto f = FMT_STRING("{} destructed\n");
  fmt::format(f, fnName());
}

shAnchor Anchor::use() {
  if (__instance__.use_count() < 1) {
    // Clock hasn't been constructed yet
    __instance__ = shAnchor(new Anchor(Host::use()));
  }

  return __instance__->shared_from_this();
}

void Anchor::chooseAnchorClock() {
  auto info = clock.info();

  warnFrequentChanges(info);
  infoNewClock(info);

  // NOTE: should confirm this is a buffered stream and AirPlay 2

  anchor_clock = data.timelineID;
  anchor_rptime = data.anchorRtpTime;
  anchor_time = data.networkTime;
  anchor_clock_new_ns = clock.now();

  // dump();
}

const anchor::ClockInfo Anchor::clockInfo() { return clock.info(); }

uint64_t Anchor::frameTimeToLocalTime(uint32_t timestamp) {
  chooseAnchorClock();

  int32_t frame_diff = timestamp - anchor_rptime;
  int64_t time_diff = frame_diff;
  time_diff *= InputInfo::rate;
  time_diff /= 1000000000;

  uint64_t ltime = anchor_time + time_diff;

  return ltime;
}

const AnchorInfo Anchor::info() {
  chooseAnchorClock();

  return AnchorInfo{.rtptime = (int64_t)anchor_rptime,
                    .networktime = (int64_t)data.networkTime,
                    .clock_id = anchor_clock,
                    .last_info_is_valid = last_info_is_valid,
                    .remote_info_is_valid = remote_info_is_valid};
}

void Anchor::save(AnchorData &ad) { std::swap(ad.calcNetTime(), data); }

void Anchor::teardown() {
  clock.teardown();
  data = AnchorData();

  // clear the local view of anchor clock
  anchor_clock = 0;
  anchor_rptime = 0;
  anchor_time = 0;
  anchor_clock_new_ns = 0;
}

// misc debug, logging

void Anchor::infoNewClock(const anchor::ClockInfo &info) {
  auto rc = false;

  // when any of these conditions are true this is a new Anchor
  rc |= (anchor_clock != data.timelineID); // anchot is different
  rc |= (anchor_rptime != data.rtpTime);   // rtpTime is different
  rc |= (anchor_time != data.networkTime); // anchor time is different

  if (rc && _debug_) {
    auto hex_fmt = FMT_STRING("{:>35}={:#x}\n");
    auto dec_fmt = FMT_STRING("{:>35}={}\n");

    fmt::print("{} anchor change\n", fnName());
    fmt::print(hex_fmt, "clock_old", anchor_clock);
    fmt::print(hex_fmt, "clock_new", data.timelineID);
    fmt::print(hex_fmt, "clock_current", info.clockID);
    fmt::print(dec_fmt, "rptTime", data.rtpTime);
    fmt::print(dec_fmt, "networkTime", data.networkTime);
    fmt::print("\n");
  }
}

void Anchor::warnFrequentChanges(const anchor::ClockInfo &info) {
  if (_debug_ == true) {
    constexpr auto threshold = Nanos(5s);

    if (data.timelineID != anchor_clock) {
      // warn if anchor clock is changing too quickly
      if (anchor_clock_new_ns != 0) {
        int64_t diff = info.now() - anchor_clock_new_ns;

        if (diff > threshold.count()) {
          constexpr auto fmt_str =
              FMT_STRING("WARN {} anchor clock changing too quickly diff={}\n");
          fmt::print(fmt_str, fnName(), diff);
        }
      }
    }
  }
}

void Anchor::dump(csrc_loc loc) const {
  const auto hex_fmt_str = FMT_STRING("{:>35}={:#x}\n");
  const auto dec_fmt_str = FMT_STRING("{:>35}={}\n");

  fmt::print("{}\n", fnName(loc));
  fmt::print(hex_fmt_str, "rat", data.rate);
  fmt::print(hex_fmt_str, "timelineID", data.timelineID);
  fmt::print(dec_fmt_str, "secs", data.secs);
  fmt::print(dec_fmt_str, "frac", data.frac);
  fmt::print(hex_fmt_str, "flags", data.flags);
  fmt::print(dec_fmt_str, "rtpTime", data.rtpTime);
  fmt::print(dec_fmt_str, "networkTime", data.networkTime);
  fmt::print(dec_fmt_str, "anchorTime", data.anchorTime);
  fmt::print(dec_fmt_str, "anchorRtpTime", data.anchorRtpTime);

  fmt::print("\n");
}

} // namespace pierre
