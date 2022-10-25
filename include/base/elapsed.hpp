/*
    Pierre - Custom Lightshow for Wiss Landing
    Copyright (C) 2022  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com

    Based, in part, on the original work of:
      http://www.pjrc.com/teensy/
      Copyright (c) 2011 PJRC.COM, LLC
*/

#pragma once

#include "base/pet.hpp"

#include <chrono>
#include <compare>
#include <cstdint>
#include <type_traits>

namespace pierre {

class Elapsed {
public:
  Elapsed(void) noexcept : nanos(pet::now_monotonic()) {}

  template <typename TO> TO as() const { return pet::as<TO>(elapsed()); }

  Elapsed &freeze() {
    nanos = elapsed();
    frozen = true;
    return *this;
  }

  const string humanize() const { return pet::humanize(elapsed()); }

  Nanos operator()() const { return elapsed(); }

  std::strong_ordering operator<=>(auto rhs) const noexcept {
    if (elapsed() < rhs) return std::strong_ordering::less;
    if (elapsed() > rhs) return std::strong_ordering::greater;

    return std::strong_ordering::equal;
  }

  void reset() { *this = Elapsed(); }

private:
  Nanos elapsed() const { return frozen ? nanos : pet::now_monotonic() - nanos; }

private:
  Nanos nanos;
  bool frozen = false;
};

} // namespace pierre
