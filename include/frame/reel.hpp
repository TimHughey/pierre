
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
#include "frame/flusher.hpp"
#include "frame/frame.hpp"

#include <algorithm>
#include <array>
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

/// @brief Container of Frames
class Reel {
  friend struct fmt::formatter<Reel>;

  using frame_container = std::deque<Frame>;

public:
  typedef frame_container::iterator iterator;
  using guard = std::unique_lock<std::mutex>;

public:
  enum kind_t : uint8_t { Empty = 0, Ready };
  using kind_desc_t = std::array<const string, 2>;
  static kind_desc_t kind_desc;

  using erased_t = int64_t;
  using remain_t = int64_t;

  using clean_results = std::pair<erased_t, remain_t>;

public:
  // construct a reel of audio frames
  // Reel() noexcept : sema(std::make_unique<sema_t>(1)) {}
  // Reel() = default;
  Reel(kind_t kind) noexcept;
  ~Reel() noexcept {}

  /// @brief Clean reel of rendered or non-rendereable frames
  /// @return clean count, avail count
  clean_results clean() noexcept;

  bool empty() noexcept {
    if (kind == Empty) return true;
    if (cached_size->load() == 0) return true;

    return false;
  }

  void frame_next(AnchorLast ancl_last, Flusher &flusher, Frame &frame) noexcept;

  ssize_t flush(Flusher &fi) noexcept;

  /// @brief Stores frame, records it's state
  /// @param frame
  void push_back(Frame &&frame) noexcept;

  /// @brief Serial number of Reel
  /// @tparam T desired return type, uint64_t (default), string
  /// @return serial number in requested format
  template <typename T = uint64_t> const T serial_num() const noexcept {
    if constexpr (std::convertible_to<T, uint64_t>) {
      return serial;
    } else if constexpr (std::same_as<T, string>) {
      return fmt::format("0x{:>04x}", serial);
    } else {
      static_assert(AlwaysFalse<T>, "unhandled reel serial num type");
      return 0;
    }
  }

  void reset(kind_t kind = Ready) noexcept;

  /// @brief Return frame count (unsafe)
  /// @return frame count
  ssize_t size() const noexcept { return (kind == Empty) ? 0 : cached_size->load(); }

  /// @brief Return frame count (safe)
  /// @return frame count
  int64_t size_cached() const noexcept { return cached_size->load(); }

private:
  guard lock() noexcept {
    auto l = guard(mtx, std::defer_lock);
    l.lock();

    return l;
  }

  ssize_t record_size() noexcept;

private:
  // order dependent
  // std::unique_ptr<sema_t> sema;
  kind_t kind{Empty};
  uint64_t serial{0};

  // order independent
  std::mutex mtx;
  frame_container frames;
  std::unique_ptr<std::atomic<ssize_t>> cached_size;

public:
  MOD_ID("frame.reel");
};

} // namespace pierre

/// @brief Custom formatter for Reel
template <> struct fmt::formatter<pierre::Reel> : formatter<std::string> {
  using Reel = pierre::Reel;

  // parse is inherited from formatter<string>.
  template <typename FormatContext> auto format(Reel &r, FormatContext &ctx) const {
    std::string msg;
    auto w = std::back_inserter(msg);

    fmt::format_to(w, "REEL 0x{:>03x} ", r.serial_num());

    if (auto s = r.size_cached(); s > 0) fmt::format_to(w, "f={:>4} ", s);

    if (r.size_cached() > 0) {

      const auto &a = r.frames.front(); // r next frame
      const auto &b = r.frames.back();  // r last frame

      fmt::format_to(w, "sn={:08x}+{:08x} ", a.sn(), b.sn());
      fmt::format_to(w, "ts={:08x}+{:08x} ", a.ts(), b.ts(1024));
    }

    fmt::format_to(w, "{:<9} ", r.kind_desc[r.kind]);

    return formatter<std::string>::format(msg, ctx);
  }
};