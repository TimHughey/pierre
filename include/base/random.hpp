//  Pierre
//  Copyright (C) 2023  Tim Hughey
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
#include "base/types.hpp"

#include <cstdint>
#include <random>

namespace pierre {

class random {
public:
  random() noexcept;
  Micros operator()(Micros min_ms, Micros max_ms) noexcept;

private:
  static std::random_device dev_rand;
  static std::mt19937 rnd32;

public:
  MOD_ID("random");
};

} // namespace pierre
