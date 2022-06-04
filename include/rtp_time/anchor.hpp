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

#include "core/typedefs.hpp"
#include "rtp_time/clock.hpp"

#include <array>
#include <fmt/format.h>
#include <memory>

namespace pierre {

namespace anchor {

using namespace std::chrono_literals;

struct Data {
  uint64_t rate = 0;
  rtp_time::ClockID clockID = 0; // aka clock id
  uint64_t secs = 0;
  uint64_t frac = 0;
  uint64_t flags = 0;
  uint64_t rtpTime = 0;
  uint64_t networkTime = 0; // from set anchor packet
  rtp_time::Nanos at_ns = rtp_time::Nanos();
  bool valid = false;
  rtp_time::Nanos valid_at_ns = rtp_time::Nanos();

  static constexpr auto NS_FACTOR = upow(10, 9);

  Data &calcNetTime();

  bool ok() const { return clockID != 0; }
  bool playing() const { return bool(rate); }
  Data &setAt();
  Data &setValid(bool val = true);

  auto validFor() const { return rtp_time::Info::nowNanos() - at_ns; }

  // return values:
  // -1 = clock is different
  //  0 = clock, rtpTime, networkTime are equal
  // +1 = clock is same, rtpTime and networkTime are different
  friend int operator<=>(const Data &lhs, const Data &rhs) {
    if (lhs.clockID != rhs.clockID) {
      return -1;
    }

    if ((lhs.clockID == rhs.clockID) &&       // clock ID same
        (lhs.rtpTime == rhs.rtpTime) &&       // rtpTime same
        (lhs.networkTime == rhs.networkTime)) // networkTime same
    {
      return 0;
    }

    return +1;
  }
};

enum Entry : size_t { ACTUAL = 0, LAST, RECENT };

constexpr auto VALID_MIN_DURATION = 5s;
constexpr Data INVALID_DATA;

} // namespace anchor

class Anchor;
typedef std::shared_ptr<Anchor> shAnchor;

class Anchor : public std::enable_shared_from_this<Anchor> {
public:
  ~Anchor();
  static shAnchor init();
  static shAnchor ptr();
  static void reset();

  const anchor::Data &cdata(anchor::Entry entry) const { return _datum[entry]; };
  anchor::Data &data(enum anchor::Entry entry) { return _datum[entry]; };
  // uint64_t frameTimeToLocalTime(uint32_t timestamp);
  const anchor::Data &getData();
  void invalidateLastIfQuickChange(const anchor::Data &data);
  bool playEnabled() const;
  void save(anchor::Data &ad);
  // anchor::Data &saveAs(const anchor::Data &ad, anchor::Entry entry);
  void teardown();

  // misc debug
  void dump(anchor::Entry entry = anchor::Entry::LAST, csrc_loc loc = src_loc::current()) const;

private:
  Anchor() { _datum.fill(anchor::Data()); }
  void infoNewClock(const rtp_time::Info &info);
  void warnFrequentChanges(const rtp_time::Info &info);

private:
  std::array<anchor::Data, 3> _datum;
  bool _is_new = false;

  // misc debug
  bool _debug_ = true;
  static constexpr auto moduleId = csv("ANCHOR");

public:
  Anchor(const Anchor &) = delete;            // no copy
  Anchor(Anchor &&) = delete;                 // no move
  Anchor &operator=(const Anchor &) = delete; // no copy assignment
  Anchor &operator=(Anchor &&) = delete;      // no move assignment
};

} // namespace pierre
