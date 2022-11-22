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

#include "base/pet.hpp"
#include "base/types.hpp"

#include <algorithm>
#include <atomic>
#include <compare>
#include <fmt/format.h>
#include <memory>
#include <optional>
#include <set>
#include <tuple>
#include <type_traits>

namespace pierre {
namespace frame {

enum state_now_t : size_t {
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
  FUTURE,
  OUTDATED,
  READY,
  RENDERED
};

class state {
public:
  state() = default;
  state(state_now_t val) noexcept : _val(val) {}
  state(const state &s) noexcept : _val(s._val.load()) {}

  state &operator=(const state &rhs) noexcept {
    _val.store(rhs._val);
    return *this;
  }

  auto &&operator=(state_now_t val) noexcept {
    _val = val;
    return *this;
  }

  // allow comparison to state_now_t, another state or a set of state_now_t
  // note:  this must be a friend implementation to align with operator<=>
  template <typename T> friend bool operator==(const state &lhs, const T &rhs) noexcept {
    if constexpr (std::is_same_v<T, state_now_t>) return lhs._val == rhs;
    if constexpr (std::is_same_v<T, state>) return lhs._val == rhs._val;
    if constexpr (std::is_same_v<T, std::set<state_now_t>>) return rhs.contains(lhs._val);

    static_assert("comparison to state_now_t, another state or a set of state_now_t only");
  }

  friend std::strong_ordering operator<=>(const state &lhs, const state &rhs) noexcept {
    return lhs._val.load() <=> rhs._val;
  }

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

  csv inspect() const noexcept;

  auto now() const noexcept { return _val.load(); }

  bool outdated() const noexcept { return *this == OUTDATED; }
  bool ready() const noexcept { return *this == READY; }

  bool ready_or_future() const noexcept {
    static const auto want = std::set{READY, FUTURE};

    return *this == want;
  }

  // returns true if the state was the wanted val
  bool store_if_equal(state_now_t want_val, state_now_t next_val) noexcept {
    return std::atomic_compare_exchange_strong(&_val, &want_val, next_val);
  }

  auto tag() noexcept { return std::make_pair("state"sv, inspect()); }

  bool update_if(const state_now_t previous, std::optional<state> &new_state) noexcept {
    auto rc = false;

    if (new_state.has_value()) {
      rc = store_if_equal(previous, new_state.value().now());
    }

    return rc;
  }

  bool updatable() const { // frame states that can be changed
    static const auto want = std::set{DSP_COMPLETE, FUTURE, READY};
    return *this == want;
  }

private:
  std::atomic<state_now_t> _val{state_now_t::NONE};
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
