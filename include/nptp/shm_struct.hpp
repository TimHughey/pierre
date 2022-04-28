
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
//
//  This work based on and inspired by
//  https://github.com/mikebrady/nqptp Copyright (c) 2021--2022 Mike Brady.

#pragma once

#include <array>
#include <pthread.h>

namespace pierre {
namespace nptp {

typedef std::array<char, 64> MasterClockIp;

struct shm_structure {
  pthread_mutex_t shm_mutex;            // for safely accessing the structure
  uint16_t version;                     // check this is equal to NQPTP_SHM_STRUCTURES_VERSION
  uint64_t master_clock_id;             // the current master clock
  MasterClockIp master_clock_ip;        // where it's coming from
  uint64_t local_time;                  // the time when the offset was calculated
  uint64_t local_to_master_time_offset; // add this to the local time to get
                                        // master clock time
  uint64_t master_clock_start_time;     // this is when the master clock became master
};

} // namespace nptp
} // namespace pierre