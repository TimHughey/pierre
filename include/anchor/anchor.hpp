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
#include <exception>
#include <fmt/format.h>
#include <memory>
#include <mutex>
#include <source_location>

#include "anchor/clock.hpp"
#include "core/host.hpp"

namespace pierre {

using mutex = std::mutex;
using runtime_error = std::runtime_error;
using src_loc = std::source_location;

typedef const char *ccs;
typedef const src_loc csrc_loc;

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

  AnchorData &calcNetTime() {
    constexpr uint64_t ns_factor = std::pow(10, 9);

    // it looks like the networkTimeFrac is a fraction where the msb is work
    // 1/2, the next 1/4 and so on now, convert the network time and fraction
    // into nanoseconds

    frac >>= 32; // reduce precision to about 1/4 nanosecond
    frac *= ns_factor;
    frac >>= 32; // now we should have nanoseconds

    // // this may become the anchor time
    networkTime = networkTime + frac;
    anchorRtpTime = rtpTime; // no backend latency, directly use rtpTime

    return *this;
  }
};

class Anchor;

typedef std::shared_ptr<Anchor> shAnchor;

class Anchor : public std::enable_shared_from_this<Anchor> {
public:
  static shAnchor use(shHost host);                       // initial creation
  static shAnchor use(csrc_loc loc = src_loc::current()); // API for Anchor

public:
  // primary public API

  void peers(const anchor::Peers &new_peers) { clock.peers(new_peers); }
  void save(AnchorData &ad) { std::swap(data.calcNetTime(), ad); }
  void teardown() { clock.teardown(); }

  // misc debug
  void dump(csrc_loc loc = src_loc::current()) const;
  ccs fnName(csrc_loc loc = src_loc::current()) const { return loc.function_name(); }

public:
  Anchor(const Anchor &) = delete;            // no copyπho
  Anchor(Anchor &&) = delete;                 // no move
  Anchor &operator=(const Anchor &) = delete; // no copy assignment
  Anchor &operator=(Anchor &&) = delete;      // no move assignment

private:
  Anchor(shHost host);

  bool _play_enabled = false;

private:
  void __init();

  // void calcNetTime();
  void chooseAnchorClock();

  void infoNewClock(const anchor::ClockInfo &info);
  void warnFrequentChanges(const anchor::ClockInfo &info);

private:
  anchor::Clock clock; // placeholder, must be initialized before use

  AnchorData data;
  AnchorData actual;

  uint64_t anchor_clock = 0;
  uint64_t anchor_rptime = 0;
  uint64_t anchor_time = 0;

  uint64_t anchor_clock_new_ns = 0;

  mutex mtx_ready;

  bool _debug_ = false;
};

} // namespace pierre
