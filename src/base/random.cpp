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

#include "base/random.hpp"

#include <algorithm>
#include <cstdint>
#include <random>
#include <utility>

namespace pierre {

std::random_device random::dev_rand;
std::mt19937 random::rnd32(random::dev_rand());

random::random() noexcept { rnd32.discard(dev_rand() % 4096); }

Micros random::operator()(Micros min_ms, Micros max_ms) noexcept {
  const auto diff = std::max(min_ms.count(), max_ms.count());

  auto rand_count = static_cast<int64_t>(rnd32() % diff);

  return Micros(std::clamp(rand_count, min_ms.count(), max_ms.count()));
}

// class random {};

} // namespace pierre