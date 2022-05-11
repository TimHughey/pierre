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

#include "clock/clock.hpp"
#include "clock/info.hpp"
#include "packet/basic.hpp"

#include <algorithm>
#include <fcntl.h>
#include <fmt/format.h>
#include <iterator>
#include <pthread.h>
#include <sys/mman.h>

namespace pierre {
namespace airplay {

using namespace boost::asio;
using namespace boost::system;
using namespace pierre::packet;

// create the Clock
Clock::Clock(const clock::Inject &di)
    : io_ctx(di.io_ctx), socket(io_ctx), address(ip::make_address(LOCALHOST)),
      endpoint(udp_endpoint(address, CTRL_PORT)) {
  shm_name = fmt::format("/{}-{}", di.service_name, di.device_id); // make shm_name

  openAndMap();
}

bool Clock::isMapped(csrc_loc loc) const {
  auto ok = (mapped && (mapped != MAP_FAILED));

  if (!ok) {
    constexpr auto f = FMT_STRING("WARN {} nqptp data is not mapped\n");
    fmt::print(f, loc.function_name());
  }

  return ok;
}

void Clock::openAndMap() {
  int shm_fd;

  if (!mapped) {
    shm_fd = shm_open(shm_name.c_str(), O_RDWR, 0);

    if (shm_fd >= 0) {
      // flags must be PROT_READ | PROT_WRITE to allow writes
      // to mapped memory for the mutex
      constexpr auto flags = PROT_READ | PROT_WRITE;
      constexpr auto bytes = sizeof(clock::shm::size());

      mapped = mmap(NULL, bytes, flags, MAP_SHARED, shm_fd, 0);

      close(shm_fd); // done with the shm memory fd
    }
  }
}

const clock::Info Clock::info() {
  if (isMapped()) { // will throw if not
    return clock::Info{0};
  }

  int prev_state;

  // prevent thread cancellation until initiialization is complete
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &prev_state);

  // shm_structure embeds a mutex at the beginning
  pthread_mutex_t *mtx = (pthread_mutex_t *)mapped;

  // use the mutex to guard while reading
  pthread_mutex_lock(mtx);

  auto *data = clock::shm::ptr(mapped);

  // populate the master IP
  auto *clock_ip = data->master_clock_ip;
  MasterClockIp master_clock_ip;
  for (size_t idx = 0; idx < master_clock_ip.size(); idx++) {
    master_clock_ip[idx] = clock_ip[idx];
  }

  // create the ClockInfo
  auto info = clock::Info{.clockID = data->master_clock_id,
                          .masterClockIp = master_clock_ip, // this will be move
                          .sampleTime = data->local_time,
                          .rawOffset = data->local_to_master_time_offset,
                          .mastershipStartTime = data->master_clock_start_time};

  // release the mutex
  pthread_mutex_unlock(mtx);

  pthread_setcancelstate(prev_state, nullptr);

  return info;
}

void Clock::peersUpdate(const Peers &new_peers) {
  if (false) { // debug
    constexpr auto f = FMT_STRING("{} new peers count={}\n");
    fmt::print(f, fnName(), new_peers.size());
  }

  if (new_peers.empty()) {
    peer_list = "T";
  } else {
    peer_list = fmt::format("T {}", fmt::join(new_peers, " ")); // create peers list
  }
}

void Clock::sendCtrlMsg(const string &msg) {
  if (socket.is_open() == false) {
    socket.async_connect(endpoint, [pending_msg = msg, this](error_code ec) {
      if (ec == errc::success) {
        sendCtrlMsg(pending_msg); // recurse

      } else {
        fmt::print("{} connect failed what={}\n", fnName(), ec.what());
        return;
      }
    });
  }

  _wire.clear();                              // clear buffer
  auto to = std::back_inserter(_wire);        // iterator to format to
  fmt::format_to(to, "{} {}", shm_name, msg); // full msg is 'shm_name msg'
  _wire.emplace_back(0x00);                   // nqptp wants a null terminated string
  auto buff = buffer(_wire);                  // lightweight buffer wrapper for async_*

  socket.async_send_to(buff, endpoint, [&](error_code ec, size_t tx_bytes) {
    if (false) {
      constexpr auto f = FMT_STRING("{} wrote ctrl msg bytes={:>03} ec={}\n");
      fmt::print(f, fnName(), tx_bytes, ec.what());
    }
  });
}

void Clock::unMap() {
  if (isMapped()) {
    munmap(mapped, clock::shm::size());

    mapped = nullptr;
  }
}

} // namespace airplay
} // namespace pierre