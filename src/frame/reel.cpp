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

#include "frame/reel.hpp"
#include "base/logger.hpp"
#include "config/config.hpp"

#include <tuple>

namespace pierre {

void Reel::add(frame_t frame) noexcept {
  if (frame->state.dsp_any()) {
    frame->reel = serial_num();
    frames.emplace_hint(frames.end(), std::make_pair(frame->timestamp, frame));
  }
}

void Reel::consume() noexcept {
  if (!frames.empty()) {
    frames.erase(frames.begin());
  } else {
    INFO(module_id, "WARN", "consume() called on reel size={}\n", size());
  }
}

bool Reel::contains(timestamp_t timestamp) noexcept {
  auto it = frames.lower_bound(timestamp);

  return it != frames.end();
}

bool Reel::flush(FlushInfo &flush) noexcept {
  auto debug = config_debug("frames.flush");

  if (frames.empty() == false) { // reel has frames, attempt to flush

    auto a = frames.begin()->second;  // reel first frame
    auto b = frames.rbegin()->second; // reel last frame

    if (flush(a) && flush(b)) {
      if (debug) INFO(module_id, "FLUSH", "{}\n", *this);
      Frames empty_map;

      std::swap(frames, empty_map);

    } else if (flush.matches<frame_t>({a, b})) {
      if (debug) INFO(module_id, "FLUSH", "SOME {}\n", *this);

      // partial match
      std::erase_if(frames, [&](const auto &item) { return flush(item.second); });
    }
  }

  return std::empty(frames);
}

} // namespace pierre