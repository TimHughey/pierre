
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
#include <exception>
#include <fmt/format.h>
#include <pthread.h>
#include <source_location>

namespace pierre {
namespace anchor {

typedef std::array<char, 64> MasterClockIp; // array based master clock ip

namespace time {
namespace shm {

using runtime_error = std::runtime_error;
using src_loc = std::source_location;
typedef const src_loc csrc_loc;

struct structure {
  pthread_mutex_t shm_mutex;            // for safely accessing the structure
  uint16_t version;                     // check version==VERSION
  uint64_t master_clock_id;             // the current master clock
  char master_clock_ip[64];             // where it's coming from
  uint64_t local_time;                  // the time when the offset was calculated
  uint64_t local_to_master_time_offset; // add this to the local time to get master clock time
  uint64_t master_clock_start_time;     // this is when the master clock became master
};

constexpr auto VERSION = 7;

constexpr size_t size() { return sizeof(structure); }

constexpr structure *ptr(void *data, csrc_loc loc = src_loc::current()) {
  auto *_ptr_ = (structure *)data;

  if (((_ptr_->version != VERSION) || (_ptr_->master_clock_id == 0))) {
    constexpr auto msg = "nqptp version mismatch";
    constexpr auto f = FMT_STRING("FAILURE {} {}\n");

    fmt::print(f, loc.function_name(), msg);
    throw(runtime_error(msg));
  }

  return _ptr_;
}

} // namespace shm
} // namespace time

// create typedef in anchor namespace
typedef time::shm::structure nqptp_t;

} // namespace anchor
} // namespace pierre