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
#include "base/anchor_data.hpp"
#include "base/input_info.hpp"
#include "base/pet.hpp"
#include "base/typical.hpp"
#include "rtp_time/master_clock.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fmt/format.h>
#include <memory>
#include <optional>

namespace pierre {

namespace shared {
std::optional<Anchor> anchor;
} // namespace shared

// destructor, singleton static functions
void Anchor::init() { shared::anchor.emplace(); }

// general API and member functions

const AnchorData &Anchor::getData() {
  auto clock_info = shared::master_clock->info();
  auto &actual = data(AnchorEntry::ACTUAL);
  auto &last = data(AnchorEntry::LAST);
  auto &recent = data(AnchorEntry::RECENT);
  auto now_ns = pet::now_nanos();

  if (clock_info.ok() == false) { // master clock doesn't exist
    return ANCHOR_INVALID_DATA;   // return default (invalid) info
  }

  // prefer the master clock when anchor clock and master clock match

  if (clock_info.clock_id == recent.clock_id) {
    // master clock is stabilizing
    if (clock_info.masterFor() < ClockInfo::AGE_MIN) {
      return ANCHOR_INVALID_DATA; // return default (invalid) data
    }

    // ok, we have master clock

    // if recent data (set via SET_ANCHOR message) isn't valid
    // nothing we can do, return INVALID_DATA
    if (recent.valid == false) {
      __LOG0(LCOL01 " invalid\n", Anchor::module_id, csv("RECENT"));
      return ANCHOR_INVALID_DATA;
    }

    // ok, we've confirmed the master clock and we have recent data

    // are we already synced to this clock?
    if (clock_info.clock_id == recent.clock_id) {
      if (clock_info.masterFor(now_ns) < 1.5s) {
        __LOG0(LCOL01 "too young {:0.3}\n", Anchor::module_id, //
               csv("MASTER"), pet::as_secs(clock_info.masterFor(now_ns)));

        return ANCHOR_INVALID_DATA;

      } else if ((last.valid == false) || (clock_info.masterFor(now_ns) > 5s)) {
        last = recent;
        last.local_time = clock_info.local_network_time(recent.network_time);
        last.setLocalTimeAt(now_ns); // capture when local time calculated

        if (is_new) {
          __LOG0(LCOL01 " valid clockId={:#x} master_for={:<0.3}\n", //
                 Anchor::module_id, csv("MASTER"), clock_info.clock_id,
                 pet::as_secs(clock_info.masterFor(now_ns)));

          is_new = false;
        }
      }

      return last; // ok, last has updated local time
    }
  }

  // the anchor clock and master clock are different
  // let's try everything to find a master even if that means falling back
  // to the master clock

  if (is_new) { // log the anchor clock has changed since
    __LOG0(LCOL01 " change isNew={} clock_id={:x} masterClockID={:x} masterFor={}\n",
           Anchor::module_id, csv("MASTER"), is_new, recent.clock_id, clock_info.clock_id,
           pet::as_secs(clock_info.masterFor(now_ns)));

    last = recent;
    last.local_time = clock_info.local_network_time(recent.network_time);
    last.setLocalTimeAt(now_ns); // capture when local time calculated

    is_new = false;

    return last;
  }

  if (last.valid && (is_new == false)) {
    if (last.sinceUpdate() > 5s) {
      recent.network_time = clock_info.local_network_time(last.network_time);

      if (clock_info.clock_id == actual.clock_id) { // original anchor clock is master again
        __LOG0(LCOL01 " matches original anchor clockId={:#x} deviation={}\n", //
               Anchor::module_id, csv("MASTER"), clock_info.clock_id,
               pet::as_secs(Nanos(recent.network_time - actual.network_time)));
      }

      recent = actual; // switch back to the actual clock
    } else {
      recent.clock_id = clock_info.clock_id;
    }

    is_new = false;
  }

  // wrose comes to worse, just return last
  return last;
}

void Anchor::invalidateLastIfQuickChange(const AnchorData &data) {
  auto &last = _datum[AnchorEntry::LAST];

  if ((data.clock_id == last.clock_id) && (last.validFor() < ANCHOR_VALID_MIN_DURATION)) {
    last.valid = false;
  }
}

void Anchor::save(AnchorData &ad) {
  if ((ad.clock_id == 0x00) || (ad.rate == 0)) {
    teardown();
    return;
  }

  ad.calcNetTime(); // always ensure network_time is populated

  auto &actual = _datum[AnchorEntry::ACTUAL];
  auto &last = _datum[AnchorEntry::LAST];
  auto &recent = _datum[AnchorEntry::RECENT];

  if ((ad <=> recent) < 0) { // this is a new anchor clock
    is_new = true;

    __LOG0(LCOL01 " clock={:#x} {}\n", module_id, csv("RECENT"), //
           ad.clock_id, ad.clock_id == last.clock_id ? csv("SAME") : csv("NEW"));
  }

  if ((ad <=> recent) > 0) { // known anchor clock details have changed
    if (last.valid && (last.validFor() < 5s)) {
      last.valid = false;

      __LOG0(LCOL01 " change before stablized clockId={:#x}\n", module_id, //
             csv("MASTER"), ad.clock_id);
    }
  }

  // ok, sanity checks complete
  recent = ad;       // save as recent (since it is)
  recent.setValid(); // mark it as valid (can be used to calculate local time)

  actual = ad;       // save as actual (to detect when anchor and master are the same)
  actual.setValid(); // also mark valid and set when it was seen

  invalidateLastIfQuickChange(ad); // if clock details have changed quickly, invalidate last

  // we have now saved the new anchor info, noted if this is a new clock_id and possibly
  // invalidated last if the clock is changing too quickly

  // deciding which clock to use for accurate network_time is handled by info()
}

void Anchor::teardown() { _datum.fill(AnchorData()); }

} // namespace pierre
