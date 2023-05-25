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
#include "base/dura.hpp"
#include "base/input_info.hpp"
#include "base/logger.hpp"
#include "base/types.hpp"
#include "clock_info.hpp"

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

// general API and member functions

AnchorLast Anchor::get_data(const ClockInfo &clk_info) noexcept {
  INFO_AUTO_CAT("get_data");

  if (clk_info.ok() == false) return AnchorLast();

  // must have source anchor data to calculate last
  if (source.has_value()) {

    if (*source == clk_info) {
      // master clock hasn't changed, just update AnchorLast
      last.update(*source, clk_info);

    } else if (last.age_check(5s)) {
      // master clock has changed relative to anchor

      INFO_AUTO("{} NEW MASTER", clk_info);

      // the anchor and master clock for a buffered session start sync'ed.
      // however, at anytime, the master clock can change while the anchor
      // remains the same.

      // to handle the master clock change, if it is stable, we update the source
      // anchor by applying it's offset to the the last localized time
      source->clock_id = clk_info.clock_id;
      dura::apply_offset(source->anchor_time, clk_info.rawOffset);
    }
  }

  return last;
}

void Anchor::reset() noexcept {
  INFO_AUTO_CAT("reset");

  string msg;
  auto w = std::back_inserter(msg);

  if (auto has_value = source.has_value(); !has_value) {
    fmt::format_to(w, "source.has_value={}", has_value);
  } else {
    fmt::format_to(w, "{}", *source);
  }

  source.reset();
  last.reset();

  INFO_AUTO("{}", msg);
}

void Anchor::save(AnchorData &&ad) noexcept {
  INFO_AUTO_CAT("save");

  INFO_AUTO("{}", ad);

  if (source.has_value()) {
    source->log_timing_change(ad);

    if (source->maybe_unstable(ad) && !last.age_check(5s)) {
      INFO_AUTO("clk_id={:#x} appears unstable", source->clock_id);
      last.reset();
    }
  }

  source.emplace(std::move(ad));
}

} // namespace pierre
