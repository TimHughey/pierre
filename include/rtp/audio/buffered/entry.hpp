
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

namespace pierre {
namespace rtp {
namespace audio {

typedef uint16_t seq_t;

struct BufferEntry { // decoded audio packets
  uint8_t ready;
  uint8_t status; // flags
  uint16_t resend_request_number;
  signed short *data;
  seq_t sequence_number;
  uint64_t initialisation_time; // the time the packet was added or the time it
                                // was noticed the packet was missing
  uint64_t resend_time;         // time of last resend request or zero
  uint32_t given_timestamp;     // for debugging and checking
  int length;                   // the length of the decoded data
};

} // namespace audio

} // namespace rtp
} // namespace pierre