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
#include "lcs/config.hpp"
#include "lcs/logger.hpp"

#include <iterator>

namespace pierre {

uint64_t Reel::next_serial_num{0x1000};

Reel::Reel(ssize_t max_frames) noexcept
    : serial(next_serial_num++), // unique serial num (for debugging)
      max_frames{max_frames},    // max frames in the reel
      consumed{0}                // number of consumed frames
{}

bool Reel::add(frame_t frame) noexcept {

  // first, is the reel full?
  auto rc = !full();

  // note: back() of empty container is undefined
  if (empty() == false) {

    // confirm the next frame is in timestamp sequence
    rc &= frame->timestamp == (back()->timestamp + 1024);
  }

  if (rc) frames.emplace_back(std::move(frame));

  return rc;
}

bool Reel::flush(FlushInfo &flush_info) noexcept {
  static constexpr csv fn_id{"flush"};

  if (empty() == false) { // reel has frames, attempt to flush

    auto a = peek_next().get(); // reel first frame
    auto b = peek_last().get(); // reel last frame

    if (flush_info(a) && flush_info(b)) {
      INFO_AUTO("{}\n", *this);
      consumed = std::ssize(frames); // simulate empty condition (this is quick)

    } else if (flush_info.matches<decltype(a)>({a, b})) {
      INFO_AUTO("SOME {}\n", *this);

      std::for_each(begin(), end(), [&, this](const auto &item) {
        if (flush_info(item)) consume();
      });
    }
  }

  return empty();
}

} // namespace pierre