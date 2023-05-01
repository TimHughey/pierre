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

enum state_now_t : size_t {
  // NOTE:  do not sort these values they are sequenced based
  //        based on frame processing and are compared via <=>
  //        in some instances
  EMPTY = 0,
  NONE,
  ERROR,
  INVALID,
  HEADER_PARSED,
  NO_SHARED_KEY,
  DECIPHER_FAILURE,
  FLUSHED,
  DECIPHERED,
  PARSE_FAILURE,
  DECODE_FAILURE,
  DECODED,
  DSP_IN_PROGRESS,
  DSP_COMPLETE,
  NO_CLOCK,
  NO_ANCHOR,
  FUTURE,
  OUTDATED,
  READY,
  RENDERED,
  SILENCE
};

class state {
public:
  state() noexcept : val{state_now_t::NONE} {}
  state(const state_now_t now_val) noexcept : val(now_val) {}

  bool deciphered() const noexcept { return *this == DECIPHERED; };
  bool decoded() const noexcept { return *this == DECODED; }

  bool dsp_any() const noexcept {
    static const auto want = std::set{DECODED, DSP_COMPLETE, DSP_IN_PROGRESS};

    return *this == want;
  }

  bool dsp_incomplete() const noexcept {
    static const auto want = std::set{DECODED, DSP_IN_PROGRESS};

    return *this == want;
  }

  bool empty() const noexcept { return *this == EMPTY; }
  bool future() const noexcept { return *this == FUTURE; }
  bool header_parsed() const noexcept { return *this == HEADER_PARSED; }

  csv inspect() const noexcept { return csv{val_to_txt_map.at(val)}; }

  auto no_clock_or_anchor() const noexcept { return (*this == NO_CLOCK) || (*this == NO_ANCHOR); }
  auto now() const noexcept { return val; }

  bool operator()(std::optional<state> &n) noexcept {
    auto changed{false};

    if (changed = n.has_value() && (n.value() != *this); changed) {
      val = n->val;
    }

    return changed;
  }

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
    } else if constexpr (std::is_same_v<T, state>) {
      return lhs.val == rhs.val;
    } else if constexpr (std::is_same_v<T, std::set<state_now_t>>) {
      return rhs.contains(lhs.val);
    } else {
      static_assert(always_false_v<T>,
                    "comparison to state_now_t, another state or a set of state_now_t only");
    }
  }

  friend std::strong_ordering operator<=>(const state &lhs, const state &rhs) noexcept {
    return lhs.val <=> rhs.val;
  }

  bool outdated() const noexcept { return *this == OUTDATED; }
  bool ready() const noexcept { return *this == READY; }

  bool ready_or_future() const noexcept {
    static const auto want = std::set{READY, FUTURE};

    return *this == want;
  }

  void record_state() const noexcept;

  auto tag() const noexcept { return std::make_pair("state", inspect().data()); }

  bool updatable() const noexcept { // frame states that can be changed
    static const auto want = std::set{DSP_COMPLETE, FUTURE, READY};
    return *this == want;
  }

private:
  state_now_t val{state_now_t::NONE};
  static std::map<state_now_t, string> val_to_txt_map;
};

} // namespace frame
} // namespace pierre

//
// Custom Formatter for state
//
template <> struct fmt::formatter<pierre::frame::state> : formatter<std::string> {

  // parse is inherited from formatter<string>.
  template <typename FormatContext>
  auto format(const pierre::frame::state &val, FormatContext &ctx) const {

    return formatter<std::string>::format(fmt::format("state={}", val.inspect()), ctx);
  }
};
