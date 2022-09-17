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
#include "base/anchor_last.hpp"
#include "base/clock_info.hpp"
#include "base/pet.hpp"
#include "base/typical.hpp"

#include <array>
#include <memory>
#include <optional>

namespace pierre {

class Anchor;
namespace shared {
extern std::optional<Anchor> anchor;
}

class Anchor {
private:
  enum Datum { source = 0, live, last, invalid, max_datum };

public:
  Anchor() {}
  static void init(); // create shared Anchor

  static frame_local_time_result_t frame_local_time_diff(const uint32_t timestamp) {
    return shared::anchor->get_data().frame_local_time_diff(timestamp);
  }

  static Nanos frame_to_local_time(const uint32_t timestamp) {
    return shared::anchor->get_data().frame_to_local_time(timestamp);
  }

  AnchorLast get_data();
  static bool rendering() { return shared::anchor->cdatum(Datum::live).rendering(); }
  void save(AnchorData &ad);
  void teardown();

  bool viable() const { return _last.has_value(); }

  // misc debug
  static void dump(Datum dat = Datum::live) {
    __LOG0(LCOL01 " dat={}\n{}\n", module_id, "DUMP", dat, shared::anchor->cdatum(dat).inspect());
  }

private:
  const AnchorData &cdatum(Datum dat) const { return _datum.at(dat); }
  AnchorData &datum(Datum dat) { return _datum[dat]; }

  void handle_new_data(const AnchorData &new_ad);
  void handle_quick_change(const AnchorData &ad);

  // misc internal debug

private:
  std::array<AnchorData, Datum::max_datum> _datum;
  std::optional<AnchorLast> _last;
  bool remote_valid = false;
  bool data_new = false;
  const bool log_data_new = true;

  // misc debug
  static constexpr auto module_id = csv("ANCHOR");

public:
  Anchor(const Anchor &) = delete;            // no copy
  Anchor(Anchor &&) = delete;                 // no move
  Anchor &operator=(const Anchor &) = delete; // no copy assignment
  Anchor &operator=(Anchor &&) = delete;      // no move assignment
};

} // namespace pierre
