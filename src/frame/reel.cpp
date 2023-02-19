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

static uint64_t NEXT_SERIAL_NUM{0x1000};

Reel::Reel() noexcept
    : serial(NEXT_SERIAL_NUM++), // unique serial num (for debugging)
      consumed{0}                // number of consumed frames
{}

void Reel::add(frame_t frame) noexcept {
  if (frame->state.dsp_any()) frames.emplace_back(frame);
}

bool Reel::flush(FlushInfo &flush) noexcept {
  static constexpr csv fn_id{"flush"};

  if (empty() == false) { // reel has frames, attempt to flush

    auto a = peek_next().get(); // reel first frame
    auto b = peek_last().get(); // reel last frame

    if (flush(a) && flush(b)) {
      INFO_AUTO("{}\n", *this);
      Frames empty;

      std::swap(frames, empty);
    } else if (flush.matches<decltype(a)>({a, b})) {
      INFO_AUTO("SOME {}\n", *this);

      std::for_each(frames.begin() + consumed, frames.end(), [&, this](const auto &item) {
        if (flush(item)) consume();
      });
    }
  }

  return empty();
}

} // namespace pierre