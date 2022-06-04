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

#include "rtp_time/anchor.hpp"
#include "core/input_info.hpp"
#include "core/typedefs.hpp"
#include "rtp_time/clock.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fmt/format.h>
#include <memory>
#include <optional>

namespace pierre {
using namespace std::chrono_literals;

namespace shared {
std::optional<shAnchor> __anchor;
std::optional<shAnchor> &anchor() { return __anchor; }
} // namespace shared

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
  uint64_t net_time_fracs = ((frac >> 32) * NS_FACTOR) >> 32;

  networkTime = secs * NS_FACTOR + net_time_fracs;

  return *this;
}

Data &Data::setAt() {
  at_ns = rtp_time::Info::nowNanos();
  return *this;
}

Data &Data::setValid(bool val) {
  valid = val;
  valid_at_ns = rtp_time::Info::nowNanos();
  return *this;
}

} // namespace anchor

Anchor::~Anchor() {
  constexpr auto f = FMT_STRING("{} {} destructed\n");
  fmt::format(f, runTicks(), fnName());
}

shAnchor Anchor::init() { return shared::anchor().emplace(new Anchor()); }
shAnchor Anchor::ptr() { return shared::anchor().value()->shared_from_this(); }
void Anchor::reset() { shared::anchor().reset(); }

// uint64_t Anchor::frameTimeToLocalTime(uint32_t timestamp) {
//   int32_t frame_diff = timestamp - _data.back().rtpTime;
//   int64_t time_diff = frame_diff;
//   time_diff *= InputInfo::rate;
//   time_diff /= upow(10, 9);

//   uint64_t ltime = _data.back().networkTime + time_diff;

//   return ltime;
// }

const anchor::Data &Anchor::getData() {
  auto clock_info = MasterClock::ptr()->getInfo();

  if (clock_info.ok() == false) { // master clock doesn't exist
    return anchor::INVALID_DATA;  // return default (invalid) info
  }

  // ok, we have master clock (will check usability later)
  // do we also have recent anchor clock details?
  auto &recent = data(anchor::Entry::RECENT);

  if (recent.valid == false) { // nope
    return recent;             // return invalid recent
  }

  auto &last = data(anchor::Entry::LAST);

  // prefer the master clock when anchor clock and master clock match
  if (clock_info.clockID == recent.clockID) {
    if (clock_info.tooYoung()) {   // whoops, master clock isn't stable
      return anchor::INVALID_DATA; // return default (invalid) data
    }

    // we know the master clock can be used (although it may not be fully stable)
    // if we don't have a last networkTime fallback to master clock
    last = recent;
    last.networkTime = clock_info.masterTime(recent.networkTime);
    last.setAt(); // note when this networkTime was calculated

    // if last was previously invalid mark as valid
    if (last.valid == false) {
      last.setValid();
    }

    if (_is_new) {
      _is_new = false; // ensure is_new flag is cleared

      if (true) { // debug
        constexpr auto f = FMT_STRING("{} {} VALID isNew={} "
                                      "clockID={} rptTime={} networkTime={}\n");
        fmt::print(f, runTicks(), fnName(), _is_new, last.clockID, last.rtpTime, last.networkTime);
      }
    }

    // ok, we have a valid anchor clock, return it
    return last;
  }

  // the anchor clock and master clock are different
  // let's try everything to find a master even if that means falling back
  // to the master clock

  if (_is_new) { // log the anchor clock has changed since it's odd
    if (true) {  // debug
      constexpr auto f = FMT_STRING("{} {} CLOCK isNew={} "
                                    "clockID={} masterClockID={}\n");
      fmt::print(f, runTicks(), fnName(), _is_new, recent.clockID, clock_info.clockID);
    }

    if (last.valid == false) {
      // we're in a tough spot... the new anchor isn't equal to the master and we don't
      // have a previous networkTime as a reference.  return this is an invalid state
      // and clear the new clock flag so we can possibly come up with a solution next
      // time getData is invoked
      _is_new = false;

      return last;
    }
  }

  // if last is valid let's use it for now by updating from the master clock
  // which we hope is fairly close to "right"
  if (last.valid && (last.validFor() == 5s)) {
    recent.networkTime = last.networkTime + clock_info.rawOffset;
    return last;
  }

  if ((last.valid == false) && (_is_new == true)) {
    // interesting situation, last isn't valid we have a new clock AND it's not
    // equal to the master clock.  something is amiss -- this isn't valid

    return last; // return last (invalid)
  }

  // ok, we have a valid last that isn't new, if enough time has past since it became
  // master then we accept the master clock as the most recent anchor
  if (last.valid) {
    recent.networkTime = last.networkTime + clock_info.rawOffset;
  }

  // wrose comes to worse, just return last
  return last;
}

void Anchor::invalidateLastIfQuickChange(const anchor::Data &data) {
  auto &last = _datum[anchor::Entry::LAST];

  if ((data.clockID == last.clockID) && (last.validFor() < anchor::VALID_MIN_DURATION)) {
    last.valid = false;
  }
}

bool Anchor::playEnabled() const { return cdata(anchor::Entry::RECENT).playing(); }

void Anchor::save(anchor::Data &ad) {
  ad.calcNetTime(); // always ensure networkTime is populated

  auto &actual = _datum[anchor::Entry::ACTUAL];
  auto &last = _datum[anchor::Entry::LAST];
  auto &recent = _datum[anchor::Entry::RECENT];

  if ((ad <=> recent) < 0) { // this is a new anchor clock
    _is_new = true;

    if (true) { // debug
      constexpr auto f = FMT_STRING("{} {} NEW clock=0x{:x}\n");
      fmt::print(f, runTicks(), moduleId, ad.clockID);
    }
  }

  if ((ad <=> recent) > 0) { // known anchor clock details have changed
    if (last.valid && (last.validFor() < 5s)) {
      last.valid = false;
      if (true) { // debug
        constexpr auto f = FMT_STRING("{} {} unstablized clock details changed "
                                      " clock=0x{:x}\n");
        fmt::print(f, runTicks(), moduleId, ad.clockID);
      }
    }
  }

  // ok, sanity checks complete
  recent = ad;               // save as recent (since it is)
  recent.setAt().setValid(); // mark it as valid (can be used to calculate local time)

  actual = ad;               // save as actual (to detect when anchor and master are the same)
  actual.setAt().setValid(); // also mark valid and set when it was seen

  invalidateLastIfQuickChange(ad); // if clock details have changed quickly, invalidate last

  // we have now saved the new anchor info, noted if this is a new clockID and possibly invalidated
  // last if the clock is changing too quickly

  // deciding which clock to use for accurate networkTime is handled by getInfo()
}

void Anchor::teardown() { _datum.fill(anchor::Data()); }

// misc debug, logging

// void Anchor::infoNewClock(const rtp_time::Info &info) {
//   auto rc = false;

//   auto &new_data = _data.back();

//   if (rc && _debug_) {
//     auto hex_fmt = FMT_STRING("{:>35}={:#x}\n");
//     auto dec_fmt = FMT_STRING("{:>35}={}\n");

//     fmt::print("{} {} anchor change\n", runTicks(), fnName());
//     fmt::print(hex_fmt, "clock_old", anchor_clock);
//     fmt::print(hex_fmt, "clock_new", _data.timelineID);
//     fmt::print(hex_fmt, "clock_current", info.clockID);
//     fmt::print(dec_fmt, "rptTime", _data.rtpTime);
//     fmt::print(dec_fmt, "networkTime", _data.networkTime);
//     fmt::print("\n");
//   }
// }

// void Anchor::warnFrequentChanges(const rtp_time::Info &info) {
//   if (_debug_ == true) {
//     constexpr auto threshold = Nanos(5s);

//     if (_data.timelineID != anchor_clock) {
//       // warn if anchor clock is changing too quickly
//       if (anchor_clock_new_ns != 0) {
//         int64_t diff = info.now() - anchor_clock_new_ns;

//         if (diff > threshold.count()) {
//           constexpr auto fmt_str =
//               FMT_STRING("WARN {} anchor clock changing too quickly diff={}\n");
//           fmt::print(fmt_str, fnName(), diff);
//         }
//       }
//     }
//   }
// }

void Anchor::dump(anchor::Entry entry, csrc_loc loc) const {
  const auto &data = cdata(entry);

  const auto hex_fmt_str = FMT_STRING("{:>35}={:#x}\n");
  const auto dec_fmt_str = FMT_STRING("{:>35}={}\n");

  fmt::print("{} {}\n", runTicks(), fnName(loc));
  fmt::print(hex_fmt_str, "rate", data.rate);
  fmt::print(hex_fmt_str, "clockID", data.clockID);
  fmt::print(dec_fmt_str, "secs", data.secs);
  fmt::print(dec_fmt_str, "frac", data.frac);
  fmt::print(hex_fmt_str, "flags", data.flags);
  fmt::print(dec_fmt_str, "rtpTime", data.rtpTime);
  fmt::print(dec_fmt_str, "networkTime", data.networkTime);
  fmt::print(dec_fmt_str, "at_ns", data.at_ns.count());
  fmt::print(dec_fmt_str, "valid", data.valid);

  fmt::print("\n");
}

} // namespace pierre
