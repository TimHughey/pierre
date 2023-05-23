//  Pierre
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

#include <array>
#include <cstdint>

namespace pierre {

/// @brief Raise integer to power
/// @param base value to raise to power
/// @param exp power
/// @return base raised to exp
consteval int64_t ipow(int64_t base, uint8_t exp) noexcept {
  if (!base) return base;
  int64_t res{1};
  while (exp) {
    if (exp & 1) res *= base;
    exp /= 2;
    base *= base;
  }
  return res;
}

/// @brief Raise 10 to power
/// @param exp exponent
/// @return 10 raised to exponent
consteval int64_t ipow10(uint8_t exp) noexcept { return ipow(10, exp); }

/// @brief Raise 10 to the specified power via lookup table
/// @param exp exponent, max 9
/// @return 10 raised to exponent
consteval int64_t qpow10(uint8_t exp) {
  constexpr std::array p10{1,       10,        100,        1000,        10'000,
                           10'0000, 1'000'000, 10'000'000, 100'000'000, 1'000'000'000};

  return p10.at(exp);
}

} // namespace pierre
