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

#include <chrono>
#include <cmath>
#include <cstdint>
#include <fmt/format.h>
#include <memory>
#include <source_location>

#include "rtp/anchor_info.hpp"

namespace pierre {
namespace rtp {

// this is overkill for this simple of a class
// goal is to have at least well-formed user object copy/assignment example

// construct with AnchorData (copy)
AnchorInfo::AnchorInfo(const AnchorData &ad) : data(ad) { __init(); }

// construct with AnchorData (move)
AnchorInfo::AnchorInfo(const AnchorData &&ad) : data(std::move(ad)) { __init(); }

// construct from AnchorInfo (copy)
AnchorInfo::AnchorInfo(const AnchorInfo &ai) : data(ai.data), actual(ai.actual) { __init(); }

// construct from AnchorInfo (move)
AnchorInfo::AnchorInfo(AnchorInfo &&ai) : data(std::move(ai.data)), actual(std::move(ai.actual)) {
  __init();
}

// assignment (copy)
AnchorInfo &AnchorInfo::operator=(const AnchorData &ad) {
  data = ad;

  __init();

  return *this;
}

// assignment (move)
AnchorInfo &AnchorInfo::operator=(AnchorData &&ad) {
  data = std::move(ad);

  __init();

  return *this;
}

// assignment (copy)
AnchorInfo &AnchorInfo::operator=(const AnchorInfo &ai) {
  data = ai.data;
  actual = ai.actual;

  __init();

  return *this;
}

// assignment (move)
AnchorInfo &AnchorInfo::operator=(AnchorInfo &&ai) {
  data = std::move(ai.data);
  actual = std::move(ai.actual);

  __init();

  return *this;
}

void AnchorInfo::__init() {
  _play_enabled = data.rate & 1;

  // if our local nptp shared_ptr isn't set use the
  // shared_ptr passed via AnchorData
  if ((nptp.use_count() == 0) && data.nptp.use_count()) {
    nptp = data.nptp;  // copy the shared_ptr locally
    data.nptp.reset(); // reset the shared_ptr passed via AnchorData
  }

  if (nptp.use_count() == 0) {
    auto fmt_str = FMT_STRING("WARN {} nptp shared ptr is not set (not refreshing ClockInfo\n");
    fmt::print(fmt_str, fnName());
  }

  calcNetTime();
  chooseAnchorClock();
}

void AnchorInfo::calcNetTime() {
  constexpr uint64_t ns_factor = std::pow(10, 9);

  uint64_t frac = data.frac;
  // it looks like the networkTimeFrac is a fraction where the msb is work
  // 1/2, the next 1/4 and so on now, convert the network time and fraction
  // into nanoseconds

  frac >>= 32; // reduce precision to about 1/4 nanosecond
  frac *= ns_factor;
  frac >>= 32; // now we should have nanoseconds

  data.networkTime = data.networkTime + frac; // this may become the anchor time
  data.anchorRtpTime = data.rtpTime;          // no backend latency, directly use rtpTime
}

void AnchorInfo::chooseAnchorClock() {
  nptp->refreshClockInfo(clock_info); // get the most recent clock info

  warnFrequentChanges();
  infoNewClock();

  // NOTE: should confirm this is a buffered stream and AirPlay 2

  anchor_clock = data.timelineID;
  anchor_rptime = data.anchorRtpTime;
  anchor_time = data.networkTime;
  anchor_clock_new_ns = clock_info.now();

  actual = data;
}

void AnchorInfo::infoNewClock() {
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
    fmt::print(hex_fmt, "clock_current", clock_info.clockID);
    fmt::print(dec_fmt, "rptTime", data.rtpTime);
    fmt::print(dec_fmt, "networkTime", data.networkTime);
    fmt::print("\n");
  }
}

void AnchorInfo::warnFrequentChanges() {
  if (_debug_ == true) {
    using namespace std::chrono_literals;
    using Nanos = std::chrono::nanoseconds;
    constexpr auto threshold = Nanos(5s);

    if (data.timelineID != anchor_clock) {
      // warn if anchor clock is changing too quickly
      if (anchor_clock_new_ns != 0) {
        int64_t diff = clock_info.now() - anchor_clock_new_ns;

        if (diff > threshold.count()) {
          auto fmt_str = FMT_STRING("WARN {} anchor clock changing too quickly diff={}\n");
          fmt::print(fmt_str, fnName(), diff);
        }
      }
    }
  }
}

void AnchorInfo::dump(const std::source_location loc) {
  const auto prefix = loc.function_name();
  const auto hex_fmt_str = FMT_STRING("{:>35}={:#x}\n");
  const auto dec_fmt_str = FMT_STRING("{:>35}={}\n");

  fmt::print("{}\n", prefix);
  fmt::print(hex_fmt_str, "rate", data.rate);
  fmt::print(hex_fmt_str, "timelineID", data.timelineID);
  fmt::print(dec_fmt_str, "secs", data.secs);
  fmt::print(dec_fmt_str, "frac", data.frac);
  fmt::print(hex_fmt_str, "flags", data.flags);
  fmt::print(dec_fmt_str, "rtpTime", data.rtpTime);
  fmt::print(dec_fmt_str, "networkTime", data.networkTime);
  fmt::print(dec_fmt_str, "anchorTime", data.anchorTime);
  fmt::print(dec_fmt_str, "anchorRtpTime", data.anchorRtpTime);
  fmt::print(dec_fmt_str, "play_enabled", _play_enabled);
  fmt::print(dec_fmt_str, "nptp.use_count()", nptp.use_count());

  fmt::print("\n");

  clock_info.dump();
}

} // namespace rtp
} // namespace pierre