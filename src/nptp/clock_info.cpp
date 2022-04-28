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

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fmt/format.h>
#include <iterator>
#include <pthread.h>
#include <source_location>
#include <vector>

#include "nptp/clock_info.hpp"
#include "nptp/shm_struct.hpp"

using namespace std::chrono;
using namespace std::chrono_literals;

namespace pierre {
namespace nptp {

ClockInfo::ClockInfo(void *shm_data) {
  _shm_data = shm_data;
  __init();
}

void ClockInfo::__init() {
  _nqptp.reserve(sizeof(shm_structure));

  copyData();
  populate();
}

void ClockInfo::dump() const {
  const auto now_ns = now();

  const int64_t now_minus_sample_time = (uint64_t)now_ns - (int64_t)sampleTime;

  const auto hex_fmt_str = FMT_STRING("{:>35}={:#x}\n");
  const auto dec_fmt_str = FMT_STRING("{:>35}={}\n");

  fmt::print("{}\n", fnName());
  fmt::print(hex_fmt_str, "clockId", clockID);
  fmt::print(dec_fmt_str, "now", now_ns);
  fmt::print(dec_fmt_str, "mastershipStart", mastershipStartTime);
  fmt::print(dec_fmt_str, "rawOffset", rawOffset);
  fmt::print(dec_fmt_str, "sampleTime", sampleTime);
  fmt::print(dec_fmt_str, "now - sampleTime", now_minus_sample_time);
  fmt::print("\n");
}

void ClockInfo::copyData() {
  // bail out if shared memory segment isn't available
  if (_shm_data == nullptr) {
    auto fmt_str = FMT_STRING("WARN {} _shm_data=nullptr\n");
    fmt::print(fmt_str, fnName());
    return;
  }

  // shm_structure embeds a mutex at the beginning
  pthread_mutex_t *mtx = (pthread_mutex_t *)_shm_data;

  // use the mutex to guard while reading
  pthread_mutex_lock(mtx);

  // copy the data
  auto to = std::back_inserter(_nqptp);
  uint8_t *data = (uint8_t *)_shm_data;
  std::copy(data, data + sizeof(shm_structure), to);

  // release the mutex
  pthread_mutex_unlock(mtx);

  // were done with _shm_data, null it out
  _shm_data = nullptr;
}

void ClockInfo::populate() {
  shm_structure *data = (shm_structure *)_nqptp.data();

  if ((data->version != VERSION) || (data->master_clock_id == 0)) {
    auto fmt_str = FMT_STRING("WARN {} version={} master_clock_id={}\n");
    fmt::print(fmt_str, fnName(), data->version, data->master_clock_id);
    return;
  }

  // these are regular uint64_t
  clockID = data->master_clock_id;
  sampleTime = data->local_time;
  rawOffset = data->local_to_master_time_offset;
  mastershipStartTime = data->master_clock_start_time;

  const auto old_ns = Clock::duration(10s).count();

  // flag if too old
  too_old = ((int64_t)sampleTime < ((int64_t)now() - (int64_t)old_ns) ? true : false);
}

void ClockInfo::refresh(void *shm_data) {
  _shm_data = shm_data;
  _nqptp.clear();

  __init();
}

} // namespace nptp
} // namespace pierre