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

#include <array>
#include <fmt/format.h>

namespace pierre {

struct FlushInfo {
  enum kind_t : uint8_t { All = 0, Normal, Inactive, Complete };

  FlushInfo() = default;

  FlushInfo(kind_t k) noexcept : kind(k) {}

  FlushInfo(seq_num_t from_sn, ftime_t from_ts, seq_num_t until_sn, ftime_t until_ts) noexcept
      : // when flush details are provided auto set kind
        kind{Normal},
        // set optional fields
        from_seq(from_sn), from_ts(from_ts),
        // flush everything <= seq_num / ts
        until_seq(until_sn), until_ts(until_ts) {}

  auto active() const noexcept { return ((kind == All) || (kind == Normal)); }

  auto all() const noexcept { return kind == All; }

  auto done() noexcept {
    from_seq = from_ts = 0;
    until_seq = until_ts = 0;
    kind = Complete;

    return flushed;
  }

  bool inactive() const noexcept { return !active(); }

  auto kind_desc() const noexcept { return kind_str[kind]; }

  bool no_accept() const noexcept { return ((kind == Inactive) || (kind == Complete)); }

  // order dependent
  kind_t kind{Inactive};
  seq_num_t from_seq{0};
  ftime_t from_ts{0};
  seq_num_t until_seq{0};
  ftime_t until_ts{0};
  int64_t flushed{0};

  static constexpr std::array kind_str{"All", "Normal", "Inactive", "Complete"};
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

    fmt::format_to(w, "{} ", fi.kind_desc());

    if (fi.from_seq) fmt::format_to(w, "*FROM sn={:<8} ts={:<12} ", fi.from_seq, fi.from_ts);
    if (fi.until_seq) fmt::format_to(w, "UNTIL sn={:<8} ts={:<12} ", fi.until_seq, fi.until_ts);
    if (fi.flushed) fmt::format_to(w, "flushed={}", fi.flushed);

    return formatter<std::string>::format(msg, ctx);
  }
};
