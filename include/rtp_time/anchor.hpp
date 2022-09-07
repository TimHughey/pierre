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

#include "base/anchor_data.hpp"
#include "base/pet.hpp"
#include "base/typical.hpp"
#include "rtp_time/master_clock.hpp"

#include <array>
#include <fmt/format.h>
#include <memory>

namespace pierre {

class Anchor;
namespace shared {
extern std::optional<Anchor> anchor;
}

class Anchor {
public:
  Anchor() { _datum.fill(AnchorData()); }

  static void init(); // create shared Anchor

  static Nanos frame_diff(const uint32_t timestamp) {
    const auto &data = shared::anchor->getData();

    return data.ok() ? data.frame_diff(timestamp) : Nanos::min();
  }

  const AnchorData &getData();
  void invalidateLastIfQuickChange(const AnchorData &data);
  static bool playEnabled() { return shared::anchor->cdata(AnchorEntry::RECENT).rendering(); }
  void save(AnchorData &ad);
  void teardown();

  // misc debug
  static void dump(auto entry = AnchorEntry::LAST, auto loc = src_loc::current()) {
    shared::anchor->cdata(entry).dump(loc);
  }

private:
  const AnchorData &cdata(AnchorEntry entry) const { return _datum[entry]; };
  AnchorData &data(enum AnchorEntry entry) { return shared::anchor->_datum[entry]; };
  void infoNewClock(const ClockInfo &info);
  void warnFrequentChanges(const ClockInfo &info);

private:
  std::array<AnchorData, 3> _datum;
  bool is_new = false;

  // misc debug
  static constexpr auto module_id = csv("ANCHOR");

public:
  Anchor(const Anchor &) = delete;            // no copy
  Anchor(Anchor &&) = delete;                 // no move
  Anchor &operator=(const Anchor &) = delete; // no copy assignment
  Anchor &operator=(Anchor &&) = delete;      // no move assignment
};

} // namespace pierre
