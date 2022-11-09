/*  Pierre - Custom Light Show for Wiss Landing
    Copyright (C) 2022  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com */

#include "frame/reel.hpp"
#include "base/logger.hpp"

#include <tuple>

namespace pierre {

void Reel::add(frame_t frame) noexcept {
  if (frame->state.dsp_any()) {
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

Reel &Reel::flush(FlushInfo &flush) noexcept {
  if (frames.empty() == false) { // nothing to flush

    // grab how many frames we currently have for debug stats (below)
    [[maybe_unused]] int64_t frames_before = std::ssize(frames);

    const auto until_seq = flush.until_seq;
    auto [a, b] = first_and_last();

    if ((until_seq >= a->seq_num) && (until_seq >= b->seq_num)) {
      INFO(module_id, "FLUSH", "clearing ALL {}\n", inspect());
      frames.clear();
    } else if ((until_seq >= a->seq_num) && (until_seq <= b->seq_num)) {
      INFO(module_id, "FLUSH", "clearing SOME {}\n", inspect());
      // partial match
      std::erase_if(frames, [&](const auto &item) {
        timestamp_t timestamp;

        std::tie(timestamp, std::ignore) = item;
        return timestamp <= flush.until_ts;
      });
    }
  }

  return *this;
}

const string Reel::inspect() const noexcept {
  string msg;
  auto w = std::back_inserter(msg);

  const auto size_now = std::ssize(frames);
  fmt::format_to(w, "{} frames={:<3}", module_id, size_now);

  if (size_now) {
    auto [front, back] = first_and_last();

    fmt::format_to(w, "seq a/b={:>8}/{:<8}", front->seq_num, back->seq_num);
    fmt::format_to(w, "ts a/b={:>12}/{:<12}", front->timestamp, back->timestamp);
  }

  return msg;
}

} // namespace pierre