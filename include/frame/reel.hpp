
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
#include "base/pet.hpp"
#include "base/types.hpp"
#include "frame/flush_info.hpp"
#include "frame/frame.hpp"

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace pierre {

using Frames = std::map<timestamp_t, frame_t>;

/// @brief Container of Frames
class Reel {
public:
  static constexpr ssize_t MAX_FRAMES = InputInfo::fps / 2;

public:
  Reel(uint64_t serial_num) noexcept
      : _serial(serial_num),                           // unique serial num (for debugging)
        module_id(fmt::format("REEL {:#5x}", _serial)) // logging prefix
  {}

  friend class Racked;
  friend struct fmt::formatter<Reel>;

  void add(frame_t frame) noexcept;
  void consume() noexcept;
  bool contains(timestamp_t timestamp) noexcept;

  bool empty() const noexcept { return frames.empty(); }

  bool flush(FlushInfo &flush) noexcept; // returns true when reel empty

  bool full() const noexcept { return std::ssize(frames) >= MAX_FRAMES; }

  friend bool operator==(const std::optional<Reel> &rhs, const uint64_t serial_num) {
    return rhs.has_value() && (rhs->_serial == serial_num);
  }

  auto peek_first() const noexcept { return frames.begin()->second; }
  auto peek_last() const noexcept { return frames.rbegin()->second; }

  template <typename T = reel_serial_num_t> const T serial_num() const noexcept {
    if constexpr (std::is_same_v<T, reel_serial_num_t>) {
      return _serial;
    } else if constexpr (std::is_same_v<T, string>) {
      return fmt::format("{:#5x}", _serial);
    } else {
      static_assert(always_false_v<T>, "unhandled reel serial num type");
    }
  }

  auto size() const noexcept { return std::ssize(frames); }

protected:
  // order dependent
  const reel_serial_num_t _serial;
  string module_id;

  Frames frames;
};

} // namespace pierre

/// @brief Custom formatter for Reel
template <> struct fmt::formatter<pierre::Reel> : formatter<std::string> {

  // parse is inherited from formatter<string>.
  template <typename FormatContext>
  auto format(const pierre::Reel &reel, FormatContext &ctx) const {
    std::string msg;
    auto w = std::back_inserter(msg);

    const auto size_now = std::ssize(reel.frames);
    fmt::format_to(w, "REEL {:#5x} frames={:<3}", reel.serial_num(), size_now);

    if (size_now) {
      auto a = reel.frames.begin()->second;  // reel first frame
      auto b = reel.frames.rbegin()->second; // reel last frame

      fmt::format_to(w, "seq a/b={:>8}/{:<8}", a->seq_num, b->seq_num);
      fmt::format_to(w, "ts a/b={:>12}/{:<12}", a->timestamp, b->timestamp);
    }

    return formatter<std::string>::format(fmt::format("{}", msg), ctx);
  }
};