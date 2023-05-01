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

#include <iterator>

namespace pierre {

uint64_t Reel::next_serial_num{0x1000};

Reel::Reel(kind_t kind) noexcept : serial(++next_serial_num), max_frames{kind} {

  if (kind == Synthetic) {
    for (auto n = 0; n < max_frames; n++) {
      frames.emplace_back(Frame());
    }
  }
}

bool Reel::can_add_frame(const Frame &frame) noexcept {
  constexpr auto fn_id{"can_add_frame"sv};

  bool rc{false};

  if (!full()) {
    rc = true;

    if (empty() == false) {
      int64_t tsp = back().timestamp;
      int64_t tsf = frame.timestamp;
      auto diff = tsf - tsp;

      if (diff != 1024) {
        INFO_AUTO("timestamp diff={}\n", diff);
        rc = false;
      }
    }
  }

  return rc;
}

bool Reel::flush(FlushInfo &flush_info) noexcept {
  static constexpr csv fn_id{"flush"};

  if (empty() == false) { // reel has frames, attempt to flush

    auto &a = front();
    auto &b = back();

    if (flush_info(a) && flush_info(b)) {
      INFO_AUTO("{}\n", *this);
      consumed = std::ssize(frames);

    } else if (flush_info.matches<decltype(a)>(a, b)) {
      INFO_AUTO("SOME {}\n", *this);

      std::for_each(begin(), end(), [&, this](const auto &item) {
        if (flush_info(item)) consume();
      });
    }
  }

  return empty();
}

Frame Reel::peek_or_pop(MasterClock &clock, Anchor &anchor) noexcept {
  static constexpr auto fn_id{"peek_front"sv};

  // NOTE: defaults to silent frame
  Frame frame;

  if (!empty() && front().synthetic()) {
    // this is a reel of synthetic frames which are always ready
    // simply return the frame at the front and mark it consumed
    frame = std::move(front());
    consume();

  } else if (!empty()) {
    // this is a reel of actual frames, get clock info and anchor
    // to calculate frame state

    // search through available frames:
    //  1. ready or future frames are returned
    //  2. outdated frames are consumed

    for (auto f = begin(); f != end() && frame.synthetic(); ++f) {
      const auto fstate = f->state_now(clock, anchor);

      // ready or future frames are returned
      if (fstate.ready_or_future()) {

        // we don't need to look at ready frames again, consume it
        if (fstate.ready()) consume();

        frame = std::move(*f);

      } else if (fstate.outdated()) {
        // eat outdated frames
        f->mark_rendered();
        consume();

      } else if (fstate.no_clock_or_anchor()) {
        break;
      } else {
        INFO_AUTO("encountered {}\n", *f);
        break;
      }
    }
  }

  // return whatever frame we found (pure synthetic, synthetic from reel or
  // actual frame)
  return frame;
}

} // namespace pierre