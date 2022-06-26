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

#include "shk.hpp"

namespace pierre {
namespace shared {
uint8v __shk;

} // namespace shared

void SharedKey::clear() { // static
  shared::__shk.clear();
}

bool SharedKey::empty() { // static
  return shared::__shk.empty();
}

const uint8_t *SharedKey::key() { // static
  return shared::__shk.raw<uint8_t>();
}

const uint8v &SharedKey::save(const uint8v &key) { // static
  shared::__shk = key;

  return shared::__shk;
}

} // namespace pierre
