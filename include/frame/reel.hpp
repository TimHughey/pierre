
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

#include "base/input_info.hpp"
#include "base/pet_types.hpp"
#include "base/types.hpp"
#include "frame/flush_info.hpp"
#include "frame/frame.hpp"

#include <iterator>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace pierre {

using Frames = std::vector<frame_t>;

/// @brief Container of Frames
class Reel {
public:
  static constexpr ssize_t MAX_FRAMES{InputInfo::fps / 2};

public:
  Reel() noexcept;
  ~Reel() = default;

  friend class Racked;
  friend struct fmt::formatter<Reel>;

  void add(frame_t frame) noexcept;
  bool consume() noexcept {
    consumed++;

    return empty();
  }

  bool empty() const noexcept {
    return std::distance(frames.begin() + consumed, frames.end()) == 0;
  }

  bool flush(FlushInfo &flush) noexcept; // returns true when reel empty

  bool full() const noexcept { return std::ssize(frames) >= MAX_FRAMES; }

  friend bool operator==(const std::optional<Reel> &rhs, const uint64_t serial_num) noexcept {
    return rhs.has_value() && (rhs->serial == serial_num);
  }

  friend bool operator<(const std::optional<Reel> &lhs, const frame_t frame) noexcept {
    auto last_frame = lhs->peek_last();

    return (frame->seq_num > last_frame->seq_num) && (frame->timestamp > last_frame->timestamp);
  }

  /// @brief The next available frame in the reel where the next Frame is adjusted
  ///        based on consumption (calls to consume())
  /// @return Raw pointer to the last frame in the reel
  frame_t peek_next() const noexcept { return *(frames.begin() + consumed); }

  /// @brief The last Frame in the reel
  /// @return
  frame_t peek_last() const noexcept { return frames.back(); }

  template <typename T = reel_serial_num_t> const T serial_num() const noexcept {
    if constexpr (std::is_same_v<T, reel_serial_num_t>) {
      return serial;
    } else if constexpr (std::is_same_v<T, string>) {
      return fmt::format("{:#5x}", serial);
    } else {
      static_assert(always_false_v<T>, "unhandled reel serial num type");
    }
  }

  auto size() const noexcept { return std::distance(frames.begin() + consumed, frames.end()); }

protected:
  // order dependent
  const reel_serial_num_t serial;
  ssize_t consumed;

  // order independent
  Frames frames;

public:
  static constexpr csv module_id{"desk.reel"};
};

} // namespace pierre

/// @brief Custom formatter for Reel
template <> struct fmt::formatter<pierre::Reel> : formatter<std::string> {

  // parse is inherited from formatter<string>.
  template <typename FormatContext>
  auto format(const pierre::Reel &reel, FormatContext &ctx) const {
    std::string msg;
    auto w = std::back_inserter(msg);

    const auto size_now = reel.size();
    fmt::format_to(w, "REEL {:#5x} frames={:<3}", reel.serial_num(), size_now);

    if (size_now) {
      auto a = reel.peek_next(); // reel next frame
      auto b = reel.peek_last(); // reel last frame

      fmt::format_to(w, "seq a/b={:>8}/{:<8}", a->seq_num, b->seq_num);
      fmt::format_to(w, "ts a/b={:>12}/{:<12}", a->timestamp, b->timestamp);
    }

    return formatter<std::string>::format(fmt::format("{}", msg), ctx);
  }
};