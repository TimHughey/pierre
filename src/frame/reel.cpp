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
#include "frame/anchor.hpp"
#include "frame/master_clock.hpp"
#include "frame/silent_frame.hpp"

#include <iterator>

namespace pierre {

uint64_t Reel::next_serial_num{0x1000};

Reel::Reel(ssize_t max_frames) noexcept
    : serial(next_serial_num++), // unique serial num (for debugging)
      max_frames{max_frames}     // max frames in the reel
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

    auto a = front().get();
    auto b = back().get();

    if (flush_info(a) && flush_info(b)) {
      INFO_AUTO("{}\n", *this);
      consumed = std::ssize(frames);

    } else if (flush_info.matches<decltype(a)>({a, b})) {
      INFO_AUTO("SOME {}\n", *this);

      std::for_each(begin(), end(), [&, this](const auto &item) {
        if (flush_info(item)) consume();
      });
    }
  }

  return empty();
}

Reel::next_frame Reel::peek_or_pop(ClockInfoConst &clk_info, Anchor *anchor) noexcept {
  static constexpr csv fn_id{"peef_front"};

  next_frame result;

  // we have frames to examine, ensure clock info is good
  if (!empty() && clk_info.ok()) {

    if (auto anchor_last = anchor->get_data(clk_info); anchor_last.ready()) {

      // search through available frames:
      //  1. ready or future frames are returned
      //  2. outdated frames are consumed

      for (auto frame_it = begin(); frame_it != end() && !result.got_frame; frame_it++) {
        auto frame = *frame_it; // get the actual frame from the iterator

        const auto fstate = frame->state_now(anchor_last);

        // ready or future frames are returned
        if (fstate.ready_or_future()) {

          // we don't need to look at ready frames again, consume it
          if (fstate.ready()) consume();

          result = next_frame(true, frame);
        } else if (fstate.outdated()) {
          // eat outdated frames
          consume();
        } else {
          INFO_AUTO("encountered {}\n", frame->inspect());
        }
      }
    } // end of anchor check
  }   // end of non-empty reel processing

  if (!result.got_frame) result.frame = SilentFrame::create();

  // we weren't able to find a useable frame
  return result;
}

} // namespace pierre