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

#include "base/types.hpp"

#include <algorithm>
#include <atomic>
#include <fmt/format.h>
#include <functional>
#include <ranges>
#include <vector>

namespace pierre {

struct FlushInfo {
  bool active{false};
  seq_num_t from_seq{0};
  timestamp_t from_ts{0};
  seq_num_t until_seq{0};
  timestamp_t until_ts{0};
  bool deferred{false};
  bool all{false};

  FlushInfo() = default;
  FlushInfo(const FlushInfo &) = default;
  FlushInfo(FlushInfo &&) = default;
  // FlushInfo &operator=(const FlushInfo &) = default;
  FlushInfo &operator=(FlushInfo &&) = default;

  constexpr FlushInfo(const seq_num_t from_seq, const timestamp_t from_ts, //
                      const seq_num_t until_seq, const timestamp_t until_ts) noexcept
      : active(true),         // enable this flush request
        from_seq(from_seq),   // set the various fields
        from_ts(from_ts),     // since this is not an aggregate class
        until_seq(until_seq), // flush everything <= seq_num
        until_ts(until_ts)    // flush everything <= timestamp
  {
    // UNSURE IF THE FOLLOWING IS CURRENT OR LEGACY
    // a flush with from[seq|ts] will not be followed by a setanchor (i.e. render)
    // if it's a flush that will be followed by a setanchor then stop render now.
  }

  static FlushInfo make_flush_all() noexcept {
    auto info = FlushInfo();
    info.active = true;
    info.all = true;

    return info;
  }

  bool operator()(seq_num_t seq_num, timestamp_t ts) const noexcept {
    return all || ((seq_num <= until_seq) && (ts <= until_ts));
  }

  bool operator()(auto &item) const noexcept {
    return all || ((item.seq_num <= until_seq) && (item.timestamp <= until_ts));
  }

  /// @brief Is this request inactive?
  /// @return true if inactive, false if active
  bool operator!() const noexcept { return !active; }

  /// @brief Determine if a list of items matches the flush criteria
  /// @tparam T Any pointer that has public class memebers seq_num and timestamp
  /// @param items A vector of items
  /// @return boolean indicating if all items match this request
  template <typename T> bool matches(const T &a, const T &b) const {

    auto check = [this](const auto &x) {
      return (x.seq_num <= until_seq) && (x.timestamp <= until_ts);
    };

    return (all || (check(a) && check(b)));
  }

  /// @brief Determine if the item passed meets the flush criteria.  If not, set the
  ///        flush request inactive.
  /// @tparam T Any pointer that has public class memebers seq_num and timestamp
  /// @param item What to examine
  /// @return boolean indicating if item meets flush request criteria
  template <typename T> bool should_flush(T &item) noexcept {

    // flush all requests are single-shot. in other words, they go inactive once
    // the initial flush is complete and do not apply to inbound frames. here we check
    // if this is a single-shot all or standard request
    if (active && !all) {
      active = ((item.seq_num <= until_seq) && (item.timestamp <= until_ts));
    } else if (all) {
      active = false;
    }

    return active;
  }
};

} // namespace pierre

/// @brief Custom formatter for FlushInfo
template <> struct fmt::formatter<pierre::FlushInfo> : formatter<std::string> {

  // parse is inherited from formatter<string>.
  template <typename FormatContext>
  auto format(const pierre::FlushInfo &info, FormatContext &ctx) const {
    std::string msg;
    auto w = std::back_inserter(msg);
    fmt::format_to(w, "{}{}", info.active ? "ACTIVE" : "INACTIVE",
                   info.deferred ? " DEFERRED " : " ");

    if (info.all) {
      fmt::format_to(w, "ALL (one-shot)");
    } else {

      if (info.from_seq || info.from_ts) {
        fmt::format_to(w, "from seq_num={:<8} timestamp={:<12} ", info.from_seq, info.from_ts);
      }

      fmt::format_to(w, "seq_num={:<8} timestamp={:<12}", info.until_seq, info.until_ts);
    }

    return formatter<std::string>::format(fmt::format("{}", msg), ctx);
  }
};
