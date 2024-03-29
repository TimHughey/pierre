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

#include "anchor.hpp"
#include "anchor_data.hpp"
#include "base/input_info.hpp"
#include "base/pet.hpp"
#include "base/types.hpp"
#include "clock_info.hpp"
#include "io/io.hpp"
#include "lcs/logger.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fmt/format.h>
#include <memory>
#include <optional>
#include <ranges>
#include <vector>

namespace pierre {

namespace shared {
std::optional<Anchor> anchor;
} // namespace shared

// destructor, singleton static functions
void Anchor::init() { shared::anchor.emplace(); }

// general API and member functions

AnchorLast Anchor::get_data(const ClockInfo &clock) noexcept {
  return shared::anchor->get_data_impl(clock);
}

AnchorLast Anchor::get_data_impl(const ClockInfo &clock) noexcept {

  // must have source anchor data to calculate last
  if (source.has_value()) {

    if (source->match_clock_id(clock)) {
      // master clock hasn't changed, just update AnchorLast
      last.update(*source, clock);

    } else if (last.age_check(5s)) {
      // master clock has changed relative to anchor
      INFO(module_id, "INFO", "new master_clock={:#x}\n", clock.clock_id);

      // the anchor and master clock for a buffered session start sync'ed.
      // at anytime, however, the master clock can change while the anchor
      // remains the same.

      // to handle the master clock change, if it is stable, we update the source
      // anchor by applying it's offset to the the last localized time
      source->clock_id = clock.clock_id;
      source->anchor_time = pet::apply_offset(last.localized, clock.rawOffset);
    }
  }

  return last;
}

void Anchor::reset() noexcept {
  shared::anchor->source.reset();
  shared::anchor->last.reset();
}

void Anchor::save(AnchorData ad) noexcept { // static

  if (shared::anchor->source.has_value()) {
    auto &source = shared::anchor->source.value();
    auto &last = shared::anchor->last;

    source.log_timing_change(ad);

    if (source.match_clock_id(ad) &&
        ((source.rtp_time != ad.rtp_time) || (source.anchor_time != ad.anchor_time))) {

      if (!last.age_check(5s)) {
        INFO(module_id, "WARN", "parameters have changed before clock={:#x} stablized\n",
             ad.clock_id);
        last.reset();
      }
    }
  }

  shared::anchor->source.emplace(ad);
}

} // namespace pierre
