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

#include "base/pe_time.hpp"
#include "base/typical.hpp"

#include <chrono>

namespace pierre {

struct FrameTimeDiff {
  // old and late must be negative values
  Nanos old{0};  // too old to be considered, must be less than late
  Nanos late{0}; // maybe playable or useable to correct out-of-sync, must be greater than old
  Nanos lead{0}; // desired lead time, positive value
};

namespace dmx {
using FPS = std::chrono::duration<double, std::ratio<1, 44>>;

// duration to wait between sending DMX frames
constexpr Nanos frame_ns() { return std::chrono::duration_cast<Nanos>(FPS(1)); }
} // namespace dmx

} // namespace pierre
