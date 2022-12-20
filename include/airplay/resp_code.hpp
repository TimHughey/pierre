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

#include <compare>
#include <map>

namespace pierre {

/// @brief RTSP response code (similar to HTTP)
class RespCode {
public:
  enum code_val : uint16_t {
    Continue = 100,
    OK = 200,
    AuthRequired = 470,
    BadRequest = 400,
    Unauthorized = 403,
    Unavailable = 451,
    InternalServerError = 500,
    NotImplemented = 501
  };

  RespCode(code_val v) noexcept : val{v} {}
  RespCode(const RespCode &) = default;
  RespCode(RespCode &&) = default;

  RespCode &operator=(const RespCode &) = default;
  RespCode &operator=(RespCode &&) = default;
  RespCode &operator=(code_val v) noexcept {
    val = v;
    return *this;
  }

  void operator()(code_val v) noexcept { val = v; }
  const string operator()() noexcept;
  auto operator<=>(const RespCode &resp_code) const = default;

  /// @brief Test equality of a RespCode to a code_val
  /// @param resp_code RespCode to test
  /// @param v code_val desired
  /// @return boolean
  friend constexpr auto operator==(const RespCode &resp_code, const code_val v) noexcept {
    return resp_code.val == v;
  }

private:
  // order dependent
  code_val val;

  // order independent
  static std::map<code_val, string> txt_map;
};
} // namespace pierre