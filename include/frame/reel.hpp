
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
#include "frame/clock_info.hpp"
#include "frame/fdecls.hpp"
#include "frame/flush_info.hpp"
#include "frame/frame.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <compare>
#include <concepts>
#include <deque>
#include <memory>
#include <mutex>
#include <set>
#include <tuple>
#include <vector>

namespace pierre {

class Reel;

using reel_t = std::unique_ptr<Reel>;
using frame_container = std::deque<Frame>;

/// @brief Container of Frames
class Reel {

  friend struct fmt::formatter<Reel>;

public:
  typedef frame_container::iterator iterator;
  using lock_guard = std::lock_guard<std::mutex>;

public:
  enum kind_t : uint8_t { Empty = 0, Starter, Transfer, Ready };

public:
  // construct a reel of audio frames
  Reel() = default;
  Reel(kind_t kind) noexcept;

  ~Reel() noexcept {}

  Reel(Reel &&) = default;
  Reel &operator=(Reel &&) = default;

  bool operator==(kind_t k) const noexcept { return k == kind; }

  const auto &back() const noexcept { return frames.back(); }
  auto begin() noexcept { return frames.begin(); }

  /// @brief Clean reel of rendered or non-rendereable frames
  /// @return clean count, avail count
  std::pair<ssize_t, ssize_t> clean() noexcept;

  void clear() noexcept;

  bool empty() const noexcept { return (frames.empty() || (kind == Empty)) ? true : false; }

  auto end() noexcept { return frames.end(); }

  auto &front() noexcept { return frames.front(); }

  ssize_t flush(FlushInfo &fi) noexcept;

  /// @brief Pop the front frame
  /// @return frames.empty()
  bool pop_front() noexcept;

  void push_back(Frame &&frame) noexcept;

  template <typename T = uint64_t> const T serial_num() const noexcept {
    if constexpr (std::same_as<T, uint64_t>) {
      return serial;
    } else if constexpr (std::same_as<T, string>) {
      return fmt::format("{:#5x}", serial);
    } else {
      static_assert(always_false_v<T>, "unhandled reel serial num type");
    }
  }

  ssize_t size() const noexcept;

  int64_t size_cached() const noexcept { return cached_size->load(); }
  ssize_t size_min() const noexcept { return min_frames; }

  bool starter() const noexcept { return kind == Starter; }

  void take(Reel &o) noexcept;

protected:
  // order dependent
  kind_t kind{Empty};
  uint64_t serial{0};
  ssize_t min_frames{0};

  // order independent
  frame_container frames;
  std::unique_ptr<std::atomic<ssize_t>> cached_size;
  std::unique_ptr<std::mutex> mtx;

public:
  static constexpr auto module_id{"frame.reel"sv};
};

} // namespace pierre

/// @brief Custom formatter for Reel
template <> struct fmt::formatter<pierre::Reel> : formatter<std::string> {

  // parse is inherited from formatter<string>.
  template <typename FormatContext> auto format(pierre::Reel &reel, FormatContext &ctx) const {
    std::string msg;
    auto w = std::back_inserter(msg);

    fmt::format_to(w, "REEL {:#05x}{}", reel.serial_num(), reel.starter() ? " STARTER" : " ");

    if (!reel.starter()) {
      fmt::format_to(w, "frames={}", reel.size());

      if (reel.frames.size() > 0) {
        const auto &a = reel.frames.front(); // reel next frame
        const auto &b = reel.frames.back();  // reel last frame

        auto delta = [](auto v1, auto v2) { return v1 > v2 ? (v1 - v2) : (v2 - v1); };

        fmt::format_to(w, " seq={}+{}", a.sn(), delta(a.sn(), b.sn()));
        fmt::format_to(w, " ts={}+{}", a.ts(1024), delta(a.ts(1024), b.ts(1024)));
      }
    }

    return formatter<std::string>::format(msg, ctx);
  }
};