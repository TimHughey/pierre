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

namespace pierre {
namespace rtp {

struct InputInfo {
  uint32_t rate = 44100; // max available at the moment
  uint8_t channels = 2;
  uint8_t bit_depth = 16;
  // uint32_t bytes_per_frame = (bit_depth * 7) / 8;
  uint32_t frame_bytes = 4; // tied to pcm output type S16LE
  size_t pcm_buffer_size = (1024 + 352) * frame_bytes;
  size_t buffer_frames = 1024;
  double lead_time = 0.1;

  size_t frameSize() const { return 352 * frame_bytes; }
  size_t wantFrames(size_t frames) const { return frameSize() * frames; }
  size_t packetSize() const { return 4096; }
};

} // namespace rtp
} // namespace pierre
