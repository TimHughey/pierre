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
#include <fmt/format.h>
#include <memory>
#include <set>

namespace pierre {
namespace frame {

// namespace {
// namespace ranges = std::ranges;
// }

enum state_now_t : size_t {
  DECIPHERED = 0,
  DECIPHER_FAILURE,
  DECODED,
  DECODE_FAILURE,
  DSP_IN_PROGRESS,
  DSP_COMPLETE,
  EMPTY,
  ERROR,
  FLUSHED,
  FUTURE,
  HEADER_PARSED,
  INVALID,
  NO_SHARED_KEY,
  NONE,
  OUTDATED,
  PARSE_FAILURE,
  RAW,
  READY,
  RENDERED
};

class state {
public:
  state() = default;
  state(state_now_t val) noexcept : _val(val) {}

  auto &&operator=(state_now_t val) noexcept {
    this->_val = val;
    return *this;
  }

  inline friend bool operator==(const state &lhs, const state_now_t &rhs) {
    return lhs._val == rhs;
  }
  inline friend bool operator==(const state &lhs, const state &rhs) { return lhs._val == rhs._val; }
  inline friend bool operator==(const state &lhs, const std::set<state_now_t> &rhs) {
    return rhs.contains(lhs._val);
  }

  bool deciphered() const { return *this == DECIPHERED; };
  bool decoded() const { return *this == DECODED; }

  bool dsp_any() const {
    static const auto want = std::set{DECODED, DSP_COMPLETE, DSP_IN_PROGRESS};

    return *this == want;
  }

  bool dsp_incomplete() const {
    static const auto want = std::set{DECODED, DSP_IN_PROGRESS};

    return *this == want;
  }

  bool empty() const { return *this == EMPTY; }
  bool future() const { return *this == FUTURE; }
  bool header_parsed() const { return *this == HEADER_PARSED; }

  csv inspect() const noexcept;

  auto now() const noexcept { return _val; }

  bool outdated() const { return *this == OUTDATED; }
  bool ready() const { return *this == READY; }

  bool ready_or_future() const noexcept {
    static const auto want = std::set{READY, FUTURE};

    return *this == want;
  }

  bool updatable() const {
    static const auto want = std::set{DSP_COMPLETE, FUTURE, READY};
    return *this == want;
  }

private:
  state_now_t _val = state_now_t::NONE;
};

} // namespace frame
} // namespace pierre

//
// Custom Formatter for state
//
template <> struct fmt::formatter<pierre::frame::state> : formatter<std::string> {

  // parse is inherited from formatter<string_view>.
  template <typename FormatContext>
  auto format(const pierre::frame::state &val, FormatContext &ctx) const {

    return formatter<std::string>::format(fmt::format("state={}", val.inspect()), ctx);
  }
};
