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

#include "base/logger.hpp"
#include "base/render.hpp"
#include "base/types.hpp"

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
  bool deferred = false;

  FlushInfo() = default;

  constexpr FlushInfo(const seq_num_t from_seq, const timestamp_t from_ts, //
                      const seq_num_t until_seq, const timestamp_t until_ts) noexcept
      : active(true),         // enable this flush request
        from_seq(from_seq),   // set the various fields
        from_ts(from_ts),     // since this is not an aggregate class
        until_seq(until_seq), // flush everything <= seq_num
        until_ts(until_ts)    // flush everything <= timestamp
  {
    // a flush with from[seq|ts] will not be followed by a setanchor (i.e. render)
    // if it's a flush that will be followed by a setanchor then stop render now.
    // if (from_seq == 0) {
    //   Render::set(false);
    // } else {
    //   deferred = true;
    // }

    INFO("FLUSH_REQUEST", "RECEIVED", "{}\n", inspect());
  }

  bool operator!() const noexcept { return !active; }

  template <typename T> bool matches(std::vector<T> items) const {
    return ranges::all_of(items, [this](auto x) { return x->seq_num <= until_seq; });
  }

  template <typename T> bool should_flush(T item) noexcept {
    if (active) {
      // use the compare result to flip active.
      active = item->seq_num <= until_seq;

      if (!active) {
        INFO("FLUSH_REQUEST", "COMPLETE", "{}\n", inspect());
      }
    }

    return active;
  }

  const string inspect() const {
    string msg;
    auto w = std::back_inserter(msg);
    fmt::format_to(w, "{} ", active ? "ACTIVE" : "INACTIVE");

    if (from_seq || from_ts) {
      fmt::format_to(w, "from seq_num={:<8} timestamp={:<12} ", from_seq, from_ts);
    }

    fmt::format_to(w, "until seq_num={:<8} timestamp={:<12}", until_seq, until_ts);

    if (deferred) fmt::format_to(w, "DEFERRED");

    return msg;
  }
};

} // namespace pierre
