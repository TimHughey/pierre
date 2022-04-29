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

#include <vector>

namespace pierre {
namespace rtp {
namespace audio {
namespace buffered {

class Raw {
public:
  Raw();

public:
  std::vector<char> buffer;

  char *toq;
  char *eoq;
  size_t max_size;
  size_t occupancy;
  pthread_mutex_t mutex;
  pthread_cond_t not_empty_cv;
  pthread_cond_t not_full_cv;

private:
  static constexpr size_t STD_PACKET_SIZE = 4096;
};

} // namespace buffered
} // namespace audio
} // namespace rtp
} // namespace pierre