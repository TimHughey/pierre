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
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <fcntl.h>
#include <fmt/format.h>
#include <iterator>
#include <pthread.h>
#include <sys/mman.h>

#include "anchor/client.hpp"
#include "anchor/clock.hpp"
#include "anchor/time/shm/struct.hpp"
#include "packet/basic.hpp"

namespace pierre {
namespace anchor {

using namespace boost::asio;
using namespace boost::system;
using namespace pierre::packet;

using io_context = boost::asio::io_context;

bool Clock::isMapped(bool throw_if_not, csrc_loc loc) const {
  auto ok = (mapped && (mapped != MAP_FAILED));

  if (!ok && throw_if_not) {
    constexpr auto f = FMT_STRING("FAILURE {} nqptp data is not mapped\n");
    fmt::print(f, loc.function_name());
    throw(runtime_error("attempted to use Clock without mapped data"));
  }

  return ok;
}

void Clock::__init() {
  [[maybe_unused]] int dont_care = PTHREAD_CANCEL_DISABLE;

  thr = jthread([&]() { runLoop(); });

  pthread_setname_np(thr.native_handle(), THREAD_NAME);
}

void Clock::openAndMap() {
  int shm_fd;

  if (isMapped(false) == false) {
    shm_fd = shm_open(shm_name.c_str(), O_RDWR, 0);

    if (shm_fd >= 0) {
      // flags must be PROT_READ | PROT_WRITE to allow writes
      // to mapped memory for the mutex
      constexpr auto flags = PROT_READ | PROT_WRITE;
      constexpr auto bytes = sizeof(time::shm::size());

      mapped = mmap(NULL, bytes, flags, MAP_SHARED, shm_fd, 0);

      close(shm_fd); // done with the shm memory fd
    }
  }
}

const ClockInfo Clock::info() {
  isMapped(); // will throw if not

  unique_lock lk_ready(mtx_ready, std::defer_lock); // ensure ready
  unique_lock lk_peers(mtx_peers, std::defer_lock); // guard changes to peers

  std::lock(lk_ready, lk_peers);

  // shm_structure embeds a mutex at the beginning
  pthread_mutex_t *mtx = (pthread_mutex_t *)mapped;

  // use the mutex to guard while reading
  pthread_mutex_lock(mtx);

  auto *data = time::shm::ptr(mapped);

  // populate the master IP
  auto *clock_ip = data->master_clock_ip;
  MasterClockIp master_clock_ip;
  for (size_t idx = 0; idx < master_clock_ip.size(); idx++) {
    master_clock_ip[idx] = clock_ip[idx];
  }

  // create the ClockInfo
  auto info = ClockInfo{.clockID = data->master_clock_id,
                        .masterClockIp = master_clock_ip, // this will be move
                        .sampleTime = data->local_time,
                        .rawOffset = data->local_to_master_time_offset,
                        .mastershipStartTime = data->master_clock_start_time};

  // release the mutex
  pthread_mutex_unlock(mtx);

  return info;
}

void Clock::runLoop() {
  static std::once_flag once_flag;
  io_context io_ctx;
  auto client = client::Session(io_ctx);
  int prev_state;

  // prevent thread cancellation until initiialization is complete
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &prev_state);

  openAndMap(); // gains access to shared memory segment

  do {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &prev_state);

    // we're ready, reset peers
    std::call_once(once_flag, [&]() {
      mtx_ready.unlock(); // must be unlocked before peersReset()
      peersReset();       // registers _shm_name with nqptp
    });

    unique_lock lk(mtx_wakeup);
    wakeup.wait(lk, [&] { return peer_list.empty() == false; });

    if (peer_list.empty() == false) {
      io_ctx.reset();
      client.sendCtrlMsg(shm_name, peer_list);
      io_ctx.run();

      unique_lock lk(mtx_peers);
      peer_list.clear(); // always clear peer list in case of spurious wake ups
    }

  } while (true);
}

void Clock::peersUpdate(const Peers &new_peers) {
  if (false) { // debug
    constexpr auto f = FMT_STRING("{} new peers count={}\n");
    fmt::print(f, fnName(), new_peers.size());
  }

  {
    unique_lock lk_ready(mtx_ready, std::defer_lock); // ensure ready
    unique_lock lk_peers(mtx_peers, std::defer_lock); // guard changes to peers

    std::lock(lk_ready, lk_peers);

    if (new_peers.empty()) {
      peer_list = "T";
    } else {
      peer_list = fmt::format("T {}", fmt::join(new_peers, " ")); // create peers list
    }
  } // lk falls out of scope and unlocks

  wakeup.notify_all();
}

void Clock::unMap() {
  isMapped(true); // throws if not

  munmap(mapped, time::shm::size());

  mapped = nullptr;
}

} // namespace anchor
} // namespace pierre