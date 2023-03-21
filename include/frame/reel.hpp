
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

#include <array>
#include <compare>
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

private:
  static constexpr ssize_t AUDIO_FRAMES{InputInfo::fps}; // two seconds

protected:
  auto cbegin() const noexcept { return frames.cbegin() + consumed; }
  auto cend() const noexcept { return frames.cend(); }
  auto begin() noexcept { return frames.begin() + consumed; }
  auto &back() noexcept { return frames.back(); }
  auto end() noexcept { return frames.end(); }
  auto &front() noexcept { return frames.front(); }

public:
  // construct a reel of audio frames
  Reel(ssize_t max_frames = AUDIO_FRAMES) noexcept;

  ~Reel() noexcept {}

  Reel(Reel &&) = default;
  Reel &operator=(Reel &&) = default;

  friend class Racked;
  friend struct fmt::formatter<Reel>;

  bool add(frame_t frame) noexcept;
  bool consume(std::ptrdiff_t count = 1) noexcept {
    consumed += count;

    return empty();
  }

  bool empty() const noexcept { return (std::ssize(frames) - consumed) <= 0; }

  virtual bool flush(FlushInfo &flush) noexcept; // returns true when reel empty

  bool full() const noexcept { return std::ssize(frames) >= max_frames; }

  auto operator<=>(const Frame &frame) noexcept {
    if (empty()) return std::weak_ordering::less;

    return *back() <=> frame;
  }

  /// @brief The next available frame in the reel where the next Frame is adjusted
  ///        based on consumption (calls to consume())
  /// @return Raw pointer to the last frame in the reel
  frame_t &peek_next() noexcept { return frames[consumed]; }

  /// @brief The last Frame in the reel
  /// @return
  virtual const frame_t &peek_last() noexcept { return back(); }

  template <typename T = uint64_t> const T serial_num() const noexcept {
    if constexpr (std::is_same_v<T, uint64_t>) {
      return serial;
    } else if constexpr (std::is_same_v<T, string>) {
      return fmt::format("{:#5x}", serial);
    } else {
      static_assert(always_false_v<T>, "unhandled reel serial num type");
    }
  }

  virtual bool silence() const noexcept { return false; }

  auto size() const noexcept { return std::ssize(frames) - consumed; }

protected:
  // order dependent
  uint64_t serial;
  ssize_t max_frames;

  std::ptrdiff_t consumed;

  // order independent
  Frames frames;

private:
  static uint64_t next_serial_num;

public:
  static constexpr csv module_id{"desk.reel"};
};

} // namespace pierre

/// @brief Custom formatter for Reel
template <> struct fmt::formatter<pierre::Reel> : formatter<std::string> {

  // parse is inherited from formatter<string>.
  template <typename FormatContext> auto format(pierre::Reel &reel, FormatContext &ctx) const {
    std::string msg;
    auto w = std::back_inserter(msg);

    const auto size_now = reel.size();
    fmt::format_to(w, "REEL {:#5x} frames={:<3}", reel.serial_num(), size_now);

    if (size_now) {
      const auto a = reel.peek_next().get(); // reel next frame
      const auto b = reel.peek_last().get(); // reel last frame

      fmt::format_to(w, "seq a/b={:>8}/{:<8}", a->seq_num, b->seq_num);
      fmt::format_to(w, " ts a/b={:>12}/{:<12}", a->timestamp, b->timestamp);
    }

    return formatter<std::string>::format(fmt::format("{}", msg), ctx);
  }
};