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

#include "base/elapsed.hpp"
#include "base/input_info.hpp"
#include "base/pet.hpp"
#include "base/types.hpp"
#include "frame/flush_info.hpp"

#include <array>
#include <memory>

namespace pierre {

using frame_ready_t = bool;
using renderable_t = bool;
using sync_wait_time_t = Nanos;
using sync_wait_valid_t = bool;

using cipher_buff_t = std::array<uint8_t, 16 * 1024>;
using cipher_buff_ptr = std::unique_ptr<cipher_buff_t>;

typedef unsigned long long ullong_t; // required by libsodium

} // namespace pierre
