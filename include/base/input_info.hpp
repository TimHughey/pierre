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

#include "base/helpers.hpp"
#include "base/pet.hpp"
#include "base/types.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

namespace pierre {

struct InputInfo {
  static constexpr uint32_t rate{44100}; // max available at the moment
  static constexpr uint8_t channels{2};
  static constexpr uint8_t bit_depth{16};
  static constexpr uint8_t bytes_per_frame{4};

  static constexpr Nanos frame{pet::from_val<Nanos>(1e+9 / rate)};

  static constexpr Nanos lead_time{frame * 1024};
  static constexpr Nanos lead_time_min{pet::percent(lead_time, 33)};

  static constexpr int fps{Millis(1000) / pet::as<Millis>(lead_time)};

  template <typename T> static constexpr auto frame_count(T v) noexcept {
    if constexpr (std::is_same_v<T, Nanos>) {
      return v / lead_time;
    } else if constexpr (IsDuration<T>) {
      return std::chrono::duration_cast<Nanos>(v) / lead_time;
    } else {
      static_assert("unhandled type");
    }
  }
};

} // namespace pierre
