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
#include "server/servers.hpp"

#include <algorithm>
#include <fcntl.h>
#include <fmt/format.h>
#include <iterator>
#include <pthread.h>
#include <sys/mman.h>
#include <utility>

namespace pierre {
namespace airplay {

namespace shared {
std::optional<shClock> __clock;
std::optional<shClock> &clock() { return __clock; }
} // namespace shared

using namespace boost::asio;
using namespace boost::system;
using namespace pierre::packet;

// create the Clock
Clock::Clock(const clock::Inject &di)
    : strand(di.io_ctx),                         // syncronize clock io
      socket(di.io_ctx),                         // create socket on strand
      address(ip::make_address(LOCALHOST)),      // endpoint address
      endpoint(udp_endpoint(address, CTRL_PORT)) // endpoint to use
{
  shm_name = fmt::format("/{}-{}", di.service_name, di.device_id); // make shm_name

  if (false) { // debug
    constexpr auto f = FMT_STRING("{} {} shm_name={} dest={}\n");
    fmt::print(f, runTicks(), fnName(), shm_name, endpoint.port());
  }
}

bool Clock::mapSharedMem() {
  if (isMapped() == false) {
    auto shm_fd = shm_open(shm_name.c_str(), O_RDWR, 0);

    if ((shm_fd < 0) && true) { // debug
      constexpr auto f = FMT_STRING("{} {} clock shm_open failed\n");
      fmt::print(f, runTicks(), fnName());
    }

    // flags must be PROT_READ | PROT_WRITE to allow writes
    // to mapped memory for the mutex
    constexpr auto flags = PROT_READ | PROT_WRITE;
    constexpr auto bytes = clock::shm::size();

    mapped = mmap(NULL, bytes, flags, MAP_SHARED, shm_fd, 0);

    close(shm_fd); // done with the shm memory fd

    if (false) { // debug
      constexpr auto f = FMT_STRING("{} {} clock mapping complete={}\n");
      fmt::print(f, runTicks(), fnName(), isMapped());
    }
  }

  return isMapped();
}

bool Clock::isMapped(csrc_loc loc) const {
  auto ok = (mapped && (mapped != MAP_FAILED));

  if (!ok) {
    constexpr auto f = FMT_STRING("{} {} nqptp data not mapped\n");
    fmt::print(f, runTicks(), loc.function_name());
  }

  return ok;
}

const clock::Info Clock::info() {
  if (mapSharedMem() == false) {
    return clock::Info{0};
  }

  int prev_state;
  clock::shm::structure data;

  // prevent thread cancellation until initiialization is complete
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &prev_state);

  // shm_structure embeds a mutex at the beginning
  pthread_mutex_t *mtx = (pthread_mutex_t *)mapped;

  // use the mutex to guard while reading
  pthread_mutex_lock(mtx);

  clock::shm::copy(mapped, &data);
  // release the mutex
  pthread_mutex_unlock(mtx);
  pthread_setcancelstate(prev_state, nullptr);

  // auto *data = clock::shm::ptr(mapped);

  // populate the master IP
  auto *clock_ip = data.master_clock_ip;
  MasterClockIp master_clock_ip;
  for (size_t idx = 0; idx < master_clock_ip.size(); idx++) {
    master_clock_ip[idx] = clock_ip[idx];
  }

  // create the ClockInfo
  auto info = clock::Info{.clockID = data.master_clock_id,
                          .masterClockIp = master_clock_ip, // this will be move
                          .sampleTime = data.local_time,
                          .rawOffset = data.local_to_master_time_offset,
                          .mastershipStartTime = data.master_clock_start_time};

  return info;
}

void Clock::peersUpdate(const Peers &new_peers) {
  if (false) { // debug
    constexpr auto f = FMT_STRING("{} {} new peers count={}\n");
    fmt::print(f, runTicks(), fnName(), new_peers.size());
  }

  if (socket.is_open() == false) { // connect, if needed
    socket.async_connect(          // comment for formatting
        endpoint,
        bind_executor(                                   // formatting comment
            strand,                                      // schedule on the strand
            [self = shared_from_this()](error_code ec) { // hold ptr to ourself
              if (ec == errc::success) {
                if (false) { // debug
                  constexpr auto f = FMT_STRING("{} CLOCK connect success\n");
                  fmt::print(f, runTicks(), ec.message());
                }
                return;
              }

              if (false) { // debug
                constexpr auto f = FMT_STRING("{} CLOCK connect failed={}\n");
                fmt::print(f, runTicks(), ec.message());
              }
            }));
  }

  // queue the update to prevent collisions
  strand.post([peers = std::move(new_peers), // avoid a copy
               self = shared_from_this()]    // hold a ptr to ourself
              {
                // build the msg: "<shm_name> T <ip_addr> <ip_addr>" + null terminator
                packet::Basic msg;
                auto where = std::back_inserter(msg);
                fmt::format_to(where, "{} T", self->shm_name);

                if (peers.empty() == false) {
                  fmt::format_to(where, " {}", fmt::join(peers, " "));
                }

                msg.emplace_back(0x00); // must be null terminated

                if (false) { // debug
                  csv peers_view((ccs)msg.data(), msg.size());
                  constexpr auto f = FMT_STRING("{} CLOCK peers={}\n");
                  fmt::print(f, runTicks(), peers_view);
                }

                error_code ec;
                auto tx_bytes = self->socket.send_to(buffer(msg),       // need asio buffer
                                                     self->endpoint, 0, // flags
                                                     ec);

                if (false) {
                  constexpr auto f = FMT_STRING("{} CLOCK send_to bytes={:>03} ec={}\n");
                  fmt::print(f, runTicks(), tx_bytes, ec.what());
                }
              });
}

void Clock::unMap() {
  if (isMapped()) {
    munmap(mapped, clock::shm::size());

    mapped = nullptr;
  }

  [[maybe_unused]] error_code ec;
  socket.shutdown(udp_socket::shutdown_both, ec);
  socket.close(ec);
}

} // namespace airplay
} // namespace pierre