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

namespace pierre {
namespace av {

constexpr std::ptrdiff_t ADTS_HEADER_SIZE{7};

/// @brief initialize the AV codec
/// @throws may throw due to allocation failures
void init();

/// @brief allocate space for the ADTS header and cipher
/// @returns pointer to memory block for encrypted data
inline uint8_t *m_buffer(cipher_buff_t &m, ptrdiff_t bytes) {
  m.emplace(ADTS_HEADER_SIZE + bytes, 0x00);

  return m->data() + ADTS_HEADER_SIZE;
}

/// @brief resizes 'm' to accomodate bytes consumed by deciphering
/// @param m buffer to resize
/// @param consumed bytes deciphered
inline void m_buffer_resize(cipher_buff_t &m, unsigned long long consumed) {
  m->resize(consumed + ADTS_HEADER_SIZE);
}

/// @brief parse (via the AV codec) the audio frame
/// @param frame frame to pase
/// @return true when parsing succeeds, otherwise false
bool parse(frame_t frame);

} // namespace av
} // namespace pierre