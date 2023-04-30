
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

#include <compare>
#include <concepts>
#include <memory>
#include <vector>

namespace pierre {

/// @brief Container of Frames
class Reel {

public:
  enum kind_t : ssize_t {
    Empty = 0,
    MaxFrames = InputInfo::fps,
    Synthetic = 500ms / InputInfo::lead_time
  };

public:
  static constexpr ssize_t DEFAULT_MAX_FRAMES{InputInfo::fps}; // two seconds

protected:
  const auto &cfront() const noexcept { return frames[consumed]; }
  auto &front() noexcept { return frames[consumed]; }
  auto &back() noexcept { return frames.back(); }
  auto begin() noexcept { return frames.begin() + consumed; }
  auto end() noexcept { return frames.end(); }

public:
  // construct a reel of audio frames
  Reel(kind_t kind = kind_t::Empty) noexcept;
  ~Reel() noexcept {}

  Reel(Reel &&) = default;
  Reel &operator=(Reel &&) = default;

  friend struct fmt::formatter<Reel>;

  auto &add(Frame &&frame) noexcept { return frames.emplace_back(std::move(frame)); }

  bool can_add_frame(const Frame &frame) noexcept;
  bool consume(std::ptrdiff_t count = 1) noexcept {
    consumed += count;

    return empty();
  }

  void clear() noexcept { consume(size()); }

  bool empty() const noexcept {
    if (!serial) return true; // default Reel is always empty

    return frames.empty() || (consumed >= size());
  }

  virtual bool flush(FlushInfo &flush) noexcept; // returns true when reel empty

  bool full() const noexcept {
    if (!serial) return true; // default Reel is always full

    return size() >= max_frames;
  }

  bool half_full() const noexcept { return size() >= (max_frames / 2); }

  auto operator<=>(const Frame &frame) noexcept {
    if (empty()) return std::weak_ordering::less;

    return back() <=> frame;
  }

  // note: clk_info must be valid
  Frame peek_or_pop(MasterClock &clock, Anchor *anchor) noexcept;

  template <typename T = uint64_t> const T serial_num() const noexcept {
    if constexpr (std::same_as<T, uint64_t>) {
      return serial;
    } else if constexpr (std::same_as<T, string>) {
      return fmt::format("{:#5x}", serial);
    } else {
      static_assert(always_false_v<T>, "unhandled reel serial num type");
    }
  }

  virtual bool silence() const noexcept { return false; }

  int64_t size() const noexcept { return serial ? std::ssize(frames) : 0; }

  bool synthetic() const noexcept { return cfront().synthetic(); }

protected:
  // order dependent
  uint64_t serial{0};
  ssize_t max_frames{0};
  std::vector<Frame> frames;

  std::ptrdiff_t consumed{0};

private:
  static uint64_t next_serial_num;

public:
  static constexpr auto module_id{"desk.reel"sv};
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

    if (reel.frames.size() > 0) {
      const auto &a = reel.frames.front(); // reel next frame
      const auto &b = reel.frames.back();  // reel last frame

      const auto a_ts = a.timestamp / 1024;
      const auto b_ts = b.timestamp / 1024;

      auto delta = [](auto &v1, auto &v2) { return v1 > v2 ? (v1 - v2) : (v2 - v1); };

      fmt::format_to(w, "seq={}+{}", a.seq_num, delta(a.seq_num, b.seq_num));
      fmt::format_to(w, " ts={}+{}", a_ts, delta(a_ts, b_ts));
    }

    return formatter<std::string>::format(fmt::format("{}", msg), ctx);
  }
};