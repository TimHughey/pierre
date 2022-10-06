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

#include "frame.hpp"
#include "types.hpp"

namespace pierre {
namespace av {
extern void check_nullptr(void *ptr);
extern void debug_dump();
extern void init(); // throws on alloc failures
extern bool keep(cipher_buff_t *m, size_t decipher_len, int used);
extern uint8_t *m_buffer(cipher_buff_ptr &m);
extern void parse(frame_t frame);

} // namespace av
} // namespace pierre