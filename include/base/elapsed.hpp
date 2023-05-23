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

#include "base/dura_t.hpp"

#include <compare>
#include <cstdint>
#include <type_traits>

namespace pierre {

/// @brief Small footprint, lightweight class to measure the passage of time
///        since object construction
class Elapsed {
public:
  Elapsed(void) noexcept : nanos(monotonic()), frozen(false) {}

  /// @brief function object to return elapsed duration as the
  ///        default precision
  /// @return Nanos
  Nanos operator()() const noexcept { return elapsed(); }

  /// @brief return the elapsed duration as an explicit type
  /// @tparam TO requested return type
  /// @return elapsed duration as requested type
  template <typename TO> inline constexpr TO as() const noexcept {
    if constexpr (std::same_as<TO, Nanos>) {
      return elapsed();
    } else if constexpr (std::same_as<TO, millis_fp>) {
      return std::chrono::duration_cast<millis_fp>(elapsed());
    } else if constexpr (std::convertible_to<TO, Nanos>) {
      return TO(elapsed());
    } else if constexpr (std::signed_integral<TO>) {
      return elapsed().count();
    } else if constexpr (std::unsigned_integral<TO>) {
      using unsigned_t = std::make_unsigned<TO>;
      return static_cast<unsigned_t>(elapsed().count());
    } else {
      static_assert(AlwaysFalse<TO>, "unsupported type");
      return 0;
    }
  }

  /// @brief Freeze the elapsed duration
  /// @return elapsed duration as std::chrono::nanoseconds
  constexpr const Nanos freeze() noexcept {
    nanos = elapsed();
    frozen = true;
    return nanos;
  }

  /// @brief Create a humanized (e.g. 1min, 20secs) of the elapsed duration
  /// @return const string
  const string humanize() const noexcept;

  /// @brief Comparison functions
  /// @param rhs
  /// @return based on comparison
  constexpr std::partial_ordering operator<=>(auto rhs) const noexcept {
    if (elapsed() < rhs) return std::partial_ordering::less;
    if (elapsed() > rhs) return std::partial_ordering::greater;

    return std::partial_ordering::equivalent;
  }

  /// @brief Reset the elapsed duration
  /// @return true (for use in if statements)
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
