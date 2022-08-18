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

#include "player/reel.hpp"
#include "base/flush_request.hpp"
#include "base/pe_time.hpp"
#include "base/typical.hpp"
#include "frame/frame.hpp"
#include "rtp_time/anchor.hpp"
#include "rtp_time/clock.hpp"

#include <algorithm>
#include <fmt/compile.h>
#include <fmt/format.h>
#include <iterator>
#include <ranges>

using namespace std::chrono_literals;

namespace pierre {
namespace player {
namespace ranges = std::ranges;

// Reel serial number (for unique identification of each Reel)
uint64_t Reel::SERIAL_NUM = 0x1000; // class level

bool Reel::flush(const FlushRequest &flush) {
  if (frames.empty()) { // nothing to flush
    return false;       // don't keep this spool
  }

  // grab how many packets we currently have for debug stats (below)
  [[maybe_unused]] int64_t frames_before = frames.size();

  const auto fseq_num = flush.until_seq;
  const auto a = frames.front();
  const auto b = frames.back();

  // spool contains flush request
  if ((fseq_num > a->seq_num) && (fseq_num <= b->seq_num)) {
    __LOG0("{:<18} FLUSH MATCH {}\n", moduleId(), inspect(shared_from_this()));

    Frames frames_keep; // frames to keep

    // the flush seq_num is in this spool. iterate the rtp packets
    // swapping over the frames to keep
    ranges::for_each(frames, [&](shFrame &frame) mutable {
      // this is a frame to keep, swap it into the new frames
      if (frame->seq_num > flush.until_seq) {
        frames_keep.emplace_back(frame->shared_from_this());
      }
    });

    // if any frames were keep then swap with object frames
    if (frames_keep.empty() == false) {
      std::swap(frames, frames_keep);
      log();
    }
  } else {
    frames.clear();
    __LOG0(LCOL01 " frames={} seq={}/{}\n", moduleId(), "DISCARDED", //
           frames_before, a->seq_num, b->seq_num);
  }

  return frames.empty() == false; // keep this spool
}

const string Reel::inspect(shReel reel) { // static
  static int64_t last_size = 0;

  string msg;
  auto w = std::back_inserter(msg);

  const auto use_count = reel.use_count();

  if (use_count == 0) {
    fmt::format_to(w, "use_count={}", use_count);
  } else {
    const int64_t size_now = reel->size();
    const int64_t diff = size_now - last_size;
    last_size = size_now;
    fmt::format_to(w, "frames={:<4} diff={:>+6} use_count={:<2}", size_now, diff, use_count);

    if (reel->size()) {
      auto front = reel->frames.front();
      auto back = reel->frames.back();

      fmt::format_to(w, " seq a/b={:>8}/{:<8}", front->seq_num, back->seq_num);
      fmt::format_to(w, " ts a/b={:>12}/{:<12}", front->timestamp, back->timestamp);
    }
  }

  return msg;
}

} // namespace player
} // namespace pierre
