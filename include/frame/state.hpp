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
#include <compare>
#include <concepts>
#include <fmt/format.h>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <tuple>
#include <type_traits>
#include <utility>

namespace pierre {
namespace frame {

enum state_now_t : uint8_t {
  // NOTE:  do not sort these values they are sequenced based
  //        values are ordered based on ability to render
  NONE = 0,
  SENTINEL,
  NO_AUDIO,
  ERROR,
  INVALID,
  NO_SHARED_KEY,
  DECIPHER_FAIL,
  PARSE_FAIL,
  DECODE_FAIL,
  HEADER_PARSED,
  FLUSHED,
  MOVED,
  OUTDATED,
  RENDERED,
  SILENCE,
  DECIPHERED,
  CAN_RENDER, // divider between renderable and not
  DSP,
  NO_CLK_ANC,
  READY,
  FUTURE
};

}

using frame_state_v = frame::state_now_t;

namespace frame {

class state {
  friend struct fmt::formatter<state>;

  auto vtxt() const noexcept { return vt_map.at(val).c_str(); }

public:
  using now_set = std::set<state_now_t>;

public:
  state() noexcept : val{NONE} {}
  state(const state_now_t now_val) noexcept : val(now_val) {}

  bool can_render() const noexcept {
    static const std::set<state_now_t> want{DSP, NO_CLK_ANC, READY, FUTURE};

    return want.contains(val);
  }

  bool flushed() const noexcept { return val == FLUSHED; }
  bool future() const noexcept { return val == FUTURE; }

  bool header_ok() const noexcept { return val == HEADER_PARSED; }

  auto now() const noexcept { return val; }

  explicit operator frame_state_v() const noexcept { return val; }

  state &operator=(const state &rhs) = default;
  state &operator=(const state_now_t now_val) noexcept {
    val = now_val;
    return *this;
  }

  // allow comparison to state_now_t, another state or a set of state_now_t
  // note:  this must be a friend implementation to align with operator<=>
  template <typename T> friend bool operator==(const state &lhs, const T &rhs) noexcept {
    if constexpr (std::same_as<T, state_now_t>) {
      return lhs.val == rhs;
    } else if constexpr (std::same_as<T, state>) {
      return lhs.val == rhs.val;
    } else if constexpr (std::same_as<T, now_set>) {
      return rhs.contains(lhs.val);
    } else {
      static_assert(always_false_v<T>, "invalid comparison");
    }
  }

  friend std::strong_ordering operator<=>(const state &lhs, const state &rhs) noexcept {
    return lhs.val <=> rhs.val;
  }

  friend std::partial_ordering operator<=>(const state &lhs, const state_now_t rhs) noexcept {
    return lhs.val <=> rhs;
  }

  bool ready_or_future() const noexcept { return (*this == READY) || (*this == FUTURE); }
  const state &record_state() const noexcept;

  bool sentinel() const noexcept { return val == frame::SENTINEL; }

  auto tag() const noexcept { return std::make_pair("state", vtxt()); }

private:
  state_now_t val{NONE};
  static std::map<state_now_t, string> vt_map;
};

} // namespace frame
} // namespace pierre

//
// Custom Formatter for state
//
template <> struct fmt::formatter<pierre::frame::state> : formatter<std::string> {

  // parse is inherited from formatter<string>.
  template <typename FormatContext>
  auto format(const pierre::frame::state &state, FormatContext &ctx) const {
    const auto msg = fmt::format("{}", state.vtxt());
    return formatter<std::string>::format(msg, ctx);
  }
};
