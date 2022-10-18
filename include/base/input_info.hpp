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

namespace pierre {

class InputInfo {
public:
  static constexpr uint32_t rate = 44100; // max available at the moment
  static constexpr uint8_t channels = 2;
  static constexpr uint8_t bit_depth = 16;
  static constexpr uint8_t bytes_per_frame = 4;

  typedef std::chrono::duration<double, std::ratio<1, rate / 1024>> InputFPS;

  static constexpr double fps() { return rate / 1024.0; }
  static constexpr MillisFP frame_ms() { return MillisFP(1000.0 / fps()); }
  template <typename T> static constexpr T frame() { return pet::cast<MillisFP, T>(frame_ms()); }
  static constexpr Nanos lead_time() { return frame<Nanos>(); }
};

} // namespace pierre
