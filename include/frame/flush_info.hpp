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

#pragma once

#include "base/typical.hpp"

#include <algorithm>
#include <fmt/format.h>
#include <ranges>
#include <vector>

namespace pierre {

struct FlushInfo {
  bool active = false;
  seq_num_t from_seq = 0;
  timestamp_t from_ts = 0;
  seq_num_t until_seq = 0;
  timestamp_t until_ts = 0;

  FlushInfo() = default;

  constexpr FlushInfo(const seq_num_t from_seq, const timestamp_t from_ts, //
                      const seq_num_t until_seq, const timestamp_t until_ts) noexcept
      : active(true),         // enable this flush request
        from_seq(from_seq),   // set the various fields
        from_ts(from_ts),     // since this is not an aggregate class
        until_seq(until_seq), // flush everything <=
        until_ts(until_ts)    //
  {}

  bool operator!() const noexcept { return !active; }

  template <typename T> bool matches(std::vector<T> items) const {
    return ranges::all_of(items, [this](auto x) { return x->seq_num <= until_seq; });
  }

  template <typename T> bool should_keep(T item) noexcept {
    if (active) {
      // use the compare result to flip active.
      active = item->seq_num <= until_seq;

      if (!active) {
        __LOG0(LCOL01 " {}\n", "FLUSH_REQUEST", "COMPLETE", inspect());
      }
    }

    return !active;
  }

  const string inspect() const {
    string msg;
    auto w = std::back_inserter(msg);

    fmt::format_to(w, "seq_num={:<8}/{:>8}", from_seq, until_seq);
    fmt::format_to(w, " timestamp={:<12}/{:>12}", from_ts, until_ts);
    fmt::format_to(w, " active={}", active);

    return msg;
  }
};

} // namespace pierre
