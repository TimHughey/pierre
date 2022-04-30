
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

#include <cstdint>
#include <memory>

namespace pierre {
namespace rtp {
namespace tcp {
namespace audio {
typedef uint16_t seq_t;

struct Entry { // decoded audio packets
  bool ready = false;
  uint8_t status; // flags
  uint16_t resend_request_number;
  signed short *data;
  seq_t sequence_number;

  // the time the packet was added or the time it
  // was noticed the packet was missing
  uint64_t initialisation_time;

  uint64_t resend_time; // time of last resend request or zero

  uint32_t given_timestamp; // for debugging and checking

  int length; // the length of the decoded data

public:
  size_t needBytes() const;

private:
  static constexpr size_t STANDARD_PACKET_SIZE = 4096;
};

} // namespace audio
} // namespace tcp
} // namespace rtp
} // namespace pierre
