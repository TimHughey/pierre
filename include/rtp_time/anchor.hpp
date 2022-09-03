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

#include "base/pet.hpp"
#include "base/typical.hpp"
#include "rtp_time/anchor/data.hpp"
#include "rtp_time/master_clock.hpp"

#include <array>
#include <fmt/format.h>
#include <memory>

namespace pierre {

class Anchor;
typedef std::shared_ptr<Anchor> shAnchor;

class Anchor : public std::enable_shared_from_this<Anchor> {
public:
  ~Anchor();
  static shAnchor init();
  static shAnchor ptr();
  static void reset();

  static Nanos frame_diff(const uint32_t timestamp) {
    const auto &data = Anchor::getData();

    return data.frame_diff(timestamp);
  }

  static const anchor::Data &getData();
  void invalidateLastIfQuickChange(const anchor::Data &data);
  static bool playEnabled();
  void save(anchor::Data &ad);
  void teardown();

  // misc debug
  static void dump(auto entry = anchor::Entry::LAST, auto loc = src_loc::current()) {
    ptr()->cdata(entry).dump(loc);
  }

private:
  Anchor() { _datum.fill(anchor::Data()); }
  const anchor::Data &cdata(anchor::Entry entry) const { return _datum[entry]; };
  static anchor::Data &data(enum anchor::Entry entry) { return ptr()->_datum[entry]; };
  void infoNewClock(const ClockInfo &info);
  void warnFrequentChanges(const ClockInfo &info);

private:
  std::array<anchor::Data, 3> _datum;
  bool _is_new = false;

  // misc debug
  static constexpr auto moduleId = csv("ANCHOR");

public:
  Anchor(const Anchor &) = delete;            // no copy
  Anchor(Anchor &&) = delete;                 // no move
  Anchor &operator=(const Anchor &) = delete; // no copy assignment
  Anchor &operator=(Anchor &&) = delete;      // no move assignment
};

} // namespace pierre
