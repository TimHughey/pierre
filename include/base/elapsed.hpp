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

#include "base/pet_types.hpp"

#include <compare>
#include <cstdint>
#include <type_traits>

namespace pierre {

class Elapsed {
public:
  Elapsed(void) noexcept : nanos(monotonic()), frozen(false) {}
  constexpr operator auto() noexcept { return elapsed(); }

  template <typename TO> TO as() const { return TO(elapsed()); }

  constexpr const Nanos freeze() noexcept {
    nanos = elapsed();
    frozen = true;
    return nanos;
  }

  const string humanize() const noexcept;

  constexpr std::strong_ordering operator<=>(auto rhs) const noexcept {
    if (elapsed() < rhs) return std::strong_ordering::less;
    if (elapsed() > rhs) return std::strong_ordering::greater;

    return std::strong_ordering::equal;
  }

  // returns true to enable use within if statements to improve precision of elapsed time
  // i.e.  if (elapsed.reset() && <something that waits>) {}
  bool reset() noexcept {
    *this = Elapsed();
    return true;
  }

private:
  static Nanos monotonic() noexcept;
  constexpr Nanos elapsed() const noexcept { return frozen ? nanos : monotonic() - nanos; }

private:
  Nanos nanos;
  bool frozen;
};

} // namespace pierre
