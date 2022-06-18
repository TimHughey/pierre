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

#include "rtp_time/anchor/data.hpp"
#include "core/input_info.hpp"
#include "core/typedefs.hpp"
#include "rtp_time/clock.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fmt/format.h>
#include <memory>
#include <optional>

namespace pierre {
using namespace std::chrono_literals;

namespace anchor {
Data &Data::calcNetTime() {
  /*
   Using PTP, here is what is necessary
     * The local (monotonic system up)time in nanos (arbitrary reference)
     * The remote (monotonic system up)time in nanos (arbitrary reference)
     * (symmetric) link delay
       1. calculate link delay (PTP)
       2. get local time (PTP)
       3. calculate remote time (nanos) wrt local time (nanos) w/PTP. Now
          we know how remote timestamps align to local ones. Now these
          network times are meaningful.
       4. determine how many nanos elapsed since anchorTime msg egress.
          Note: remote monotonic nanos for iPhones stops when they sleep, though
          not when casting media.
   */
  uint64_t net_time_fracs = 0;

  net_time_fracs >>= 32;
  net_time_fracs *= rtp_time::NS_FACTOR.count();
  net_time_fracs >>= 32;

  networkTime = secs * rtp_time::NS_FACTOR.count() + net_time_fracs;

  return *this;
}

Nanos Data::frameLocalTime(uint32_t timestamp) const {
  Nanos local_time{0};

  if (valid) {
    int32_t diff_frame = timestamp - rtpTime;
    int64_t diff_ts = (diff_frame * rtp_time::NS_FACTOR.count()) / InputInfo::rate;

    local_time = localTime + Nanos(diff_ts);
  }

  return local_time;
}

Seconds Data::netTimeElapsed() const {
  return std::chrono::duration_cast<Seconds>(netTimeNow() - valid_at_ns);
}

Nanos Data::netTimeNow() const {
  auto diff_steady = rtp_time::nowNanos() - valid_at_ns;

  return valid_at_ns + diff_steady;
}

Data &Data::setAt() {
  at_nanos = rtp_time::nowNanos();
  return *this;
}

Data &Data::setLocalTimeAt(Nanos local_at) {
  localAtNanos = local_at;
  return *this;
}

Data &Data::setValid(bool set_valid) {
  valid = set_valid;
  valid_at_ns = rtp_time::nowNanos();
  return *this;
}

// misc debug
void Data::dump(csrc_loc loc) const {
  const auto hex_fmt_str = FMT_STRING("{:>35}={:#x}\n");
  const auto dec_fmt_str = FMT_STRING("{:>35}={}\n");

  string msg;
  auto w = std::back_inserter(msg);

  fmt::format_to(w, "{}\n", fnName(loc));
  fmt::format_to(w, hex_fmt_str, "rate", rate);
  fmt::format_to(w, hex_fmt_str, "clockID", clockID);
  fmt::format_to(w, dec_fmt_str, "secs", secs);
  fmt::format_to(w, dec_fmt_str, "frac", frac);
  fmt::format_to(w, hex_fmt_str, "flags", flags);
  fmt::format_to(w, dec_fmt_str, "rtpTime", rtpTime);
  fmt::format_to(w, dec_fmt_str, "networkTime", networkTime);
  fmt::format_to(w, dec_fmt_str, "at_nanos", at_nanos);
  fmt::format_to(w, dec_fmt_str, "localAtNanos", localAtNanos);
  fmt::format_to(w, dec_fmt_str, "valid", valid);
  fmt::format_to(w, dec_fmt_str, "valid_at_ns", valid_at_ns);

  __LOG0("{}\n", msg);
}

} // namespace anchor
} // namespace pierre
