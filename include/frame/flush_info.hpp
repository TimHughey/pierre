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

#include <atomic>
#include <fmt/format.h>
#include <functional>
#include <mutex>
#include <set>
#include <utility>

namespace pierre {

class FlushInfo {
  //// NOTE:
  ////
  //// UNSURE IF THE FOLLOWING IS CURRENT OR LEGACY
  //// a flush with from[seq|ts] will not be followed by a setanchor (i.e. render)
  //// if it's a flush that will be followed by a set_anchor then stop render now.

public:
  enum kind_t : uint8_t { All = 0, Normal, Inactive, Complete };

  using a_int64_t = std::atomic_int64_t;
  using atomic_kind_t = std::atomic_uint8_t;
  using mutex_t = std::mutex;
  using lock_t = std::unique_lock<mutex_t>;

  friend struct fmt::formatter<FlushInfo>;

public:
  FlushInfo(kind_t want_kind = Inactive) noexcept;
  FlushInfo(seq_num_t from_sn, ftime_t from_ts, seq_num_t until_sn, ftime_t until_ts) noexcept;

  FlushInfo(const FlushInfo &) = default;
  FlushInfo(FlushInfo &&) = default;

  FlushInfo &operator=(FlushInfo &&) = default;

  /// @brief Replace this with a newly created flush request
  ///        flush specifics are copied and kind is set (All, Normal)
  /// @param next_fi flush details
  [[nodiscard]] lock_t accept(FlushInfo &&fi) noexcept;

  /// @brief Waits for in-progress flush request
  void busy_hold() noexcept;

  /// @brief Determine if the frame should be kept
  /// @tparam T Any object that has seq_num and timestamp data members
  /// @param frame What to examine
  /// @return boolean indicating if frame meets flush criteria
  template <typename T>
    requires IsFrame<T>
  bool check(T &frame) noexcept {
    static const std::set<kind_t> always_false{Inactive, Complete};

    if (always_false.contains(kind)) return false;
    if (kind == All) return true;

    auto flush{true};
    flush &= std::cmp_less_equal(frame.sn(), until_seq);
    flush &= std::cmp_less_equal(frame.ts(), until_ts);

    if (flush) std::atomic_fetch_add(a_count.get(), 1);

    return flush;
  }

  /// @brief Reset count of frames flushed
  /// @return number of frames frames
  auto count() noexcept { return std::atomic_exchange(a_count.get(), 0); }

  /// @brief Mark this flush as finished (active() == false)
  void done(lock_t &l) noexcept;

  auto kind_string() const noexcept {
    static const std::array ks{"All", "Normal", "Inactive", "Complete"};

    return ks[kind];
  }

private:
  // order dependent
  kind_t kind;
  seq_num_t from_seq{0};
  ftime_t from_ts{0};
  seq_num_t until_seq{0};
  ftime_t until_ts{0};

  std::unique_ptr<mutex_t> mtx;
  std::unique_ptr<std::atomic_int64_t> a_count{nullptr};

  static constexpr auto module_id{"desk.flush"sv};
};

} // namespace pierre

/// @brief Custom formatter for FlushInfo
template <> struct fmt::formatter<pierre::FlushInfo> : fmt::formatter<std::string> {
  using FlushInfo = pierre::FlushInfo;

  // parse is inherited from formatter<string>.
  template <typename FormatContext>
  auto format(const pierre::FlushInfo &fi, FormatContext &ctx) const {
    auto msg = fmt::format("FLUSH_INFO ");
    auto w = std::back_inserter(msg);

    fmt::format_to(w, "{} ", fi.kind_string());

    if (fi.from_seq) fmt::format_to(w, "*FROM sn={:<8} ts={:<12} ", fi.from_seq, fi.from_ts);
    if (fi.until_seq) fmt::format_to(w, "UNTIL sn={:<8} ts={:<12} ", fi.until_seq, fi.until_ts);

    auto fcount = (bool)fi.a_count ? fi.a_count->load() : int64_t{0};
    if (fcount) fmt::format_to(w, "flushed={}", fcount);

    return formatter<std::string>::format(msg, ctx);
  }
};
