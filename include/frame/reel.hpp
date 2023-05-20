
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
#include <semaphore>
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

  using sema_t = std::binary_semaphore;

public:
  typedef frame_container::iterator iterator;
  using lock_t = std::unique_lock<std::mutex>;

public:
  enum kind_t : uint8_t { Empty = 0, Starter, Transfer, Ready, EndOfKinds };

  using kind_desc_t = std::array<const string, EndOfKinds>;
  static kind_desc_t kind_desc;

  using min_sizes_t = std::array<int64_t, EndOfKinds>;
  static min_sizes_t min_sizes;

  using erased_t = int64_t;
  using remain_t = int64_t;

  using clean_results = std::pair<erased_t, remain_t>;

public:
  // construct a reel of audio frames
  // Reel() noexcept : sema(std::make_unique<sema_t>(1)) {}
  // Reel() = default;
  Reel(kind_t kind) noexcept;

  ~Reel() noexcept {}

  Reel(Reel &&) = default;
  Reel &operator=(Reel &&) = default;

  bool operator==(kind_t k) const noexcept { return k == kind; }

  /// @brief Clean reel of rendered or non-rendereable frames
  /// @return clean count, avail count
  clean_results clean() noexcept;

  void clear() noexcept;

  bool empty() noexcept {
    if (kind == Empty) return true;
    if (cached_size->load() == 0) return true;

    return false;
  }

  auto &front() noexcept { return frames.front(); }

  ssize_t flush(Flusher &fi) noexcept;

  /// @brief Pop the front frame
  /// @return frames.empty()
  bool pop_front() noexcept;

  /// @brief Stores frame, records it's state
  /// @param frame
  void push_back(Frame &&frame) noexcept;

  void push_back(Reel &o_reel) noexcept;

  /// @brief Serial number of Reel
  /// @tparam T desired return type, uint64_t (default), string
  /// @return serial number in requested format
  template <typename T = uint64_t> const T serial_num() const noexcept {
    if constexpr (std::convertible_to<T, uint64_t>) {
      return serial;
    } else if constexpr (std::same_as<T, string>) {
      return fmt::format("0x{:>04x}", serial);
    } else {
      static_assert(always_false_v<T>, "unhandled reel serial num type");
      return 0;
    }
  }

  void reset(kind_t kind = Transfer) noexcept;

  /// @brief Return frame count (unsafe)
  /// @return frame count
  ssize_t size() const noexcept { return (kind == Empty) ? 0 : cached_size->load(); }

  /// @brief Return frame count (safe)
  /// @return frame count
  int64_t size_cached() const noexcept { return cached_size->load(); }

  /// @brief Minimum frames required
  /// @return frame count
  ssize_t size_min() const noexcept { return min_sizes[kind]; }

  /// @brief Transfer src to this reel and lock the reel in prep for flushing
  /// @param arc Destination of the src reel
  void transfer(Reel &src) noexcept;

protected:
  // order dependent
  // std::unique_ptr<sema_t> sema;
  kind_t kind{Empty};
  uint64_t serial{0};

  // order independent
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
    if (auto n = r.size_min(); n > 1) fmt::format_to(w, "min={:>2}", n);

    return formatter<std::string>::format(msg, ctx);
  }
};