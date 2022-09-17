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

#include "master_clock.hpp"
#include "base/typical.hpp"
#include "base/uint8v.hpp"
#include "config/config.hpp"
#include "core/host.hpp"

#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <iterator>
#include <pthread.h>
#include <ranges>
#include <sys/mman.h>
#include <time.h>
#include <utility>

namespace pierre {

namespace shared {
std::optional<MasterClock> master_clock;
} // namespace shared

// create the MasterClock
MasterClock::MasterClock(io_context &io_ctx) noexcept
    : local_strand(io_ctx),                                          // syncronize clock io
      socket(io_ctx, ip_udp::v4()),                                  // construct and open
      remote_endpoint(asio::ip::make_address(LOCALHOST), CTRL_PORT), // nqptp endpoint
      shm_name(
          fmt::format("/{}-{}", Config::receiverName(), Host::ptr()->deviceID())) // make shm_name
{
  __LOG0(LCOL01 " shm_name={} dest={}:{}\n", module_id, "CONSTRUCT", //
         shm_name, remote_endpoint.address().to_string(), remote_endpoint.port());
}

// NOTE: new data is available every 126ms
const ClockInfo MasterClock::info() {
  if (mapSharedMem() == false) {
    return ClockInfo();
  }

  int prev_state; // to restore pthread cancel state

  // prevent thread cancellation while copying
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &prev_state);

  // shm_structure embeds a mutex at the beginning
  pthread_mutex_t *mtx = (pthread_mutex_t *)mapped;

  // use the mutex to guard while reading
  pthread_mutex_lock(mtx);

  nqptp data;
  memcpy(&data, (char *)mapped, sizeof(nqptp));
  if (data.version != NQPTP_VERSION) {
    static string msg;
    fmt::format_to(std::back_inserter(msg), "nqptp version mismatch vsn={}", data.version);
    __LOG0(LCOL01 " {}\n", module_id, "FATAL", msg);
    throw(std::runtime_error(msg.c_str()));
  }

  // release the mutex
  pthread_mutex_unlock(mtx);
  pthread_setcancelstate(prev_state, nullptr);

  // find the clock IP string
  string_view clock_ip_sv = string_view(data.master_clock_ip, sizeof(nqptp::master_clock_ip));
  auto trim_pos = clock_ip_sv.find_first_of('\0');
  clock_ip_sv.remove_suffix(clock_ip_sv.size() - trim_pos);

  return ClockInfo // calculate a local view of the nqptp data
      {.clock_id = data.master_clock_id,
       .masterClockIp = string(clock_ip_sv),
       .sampleTime = data.local_time,
       .rawOffset = data.local_to_master_time_offset,
       .mastershipStartTime = pet::from_ns(data.master_clock_start_time)};
}

void MasterClock::init(io_context &io_ctx) {

  if (shared::master_clock.has_value() == false) {
    auto &master_clock = shared::master_clock.emplace(io_ctx);

    master_clock.peersReset(); // reset the peers (and creates the shm mem name)
  }
}

bool MasterClock::isMapped() const {
  static uint32_t attempts = 0;
  auto ok = (mapped && (mapped != MAP_FAILED));

  if (!ok && attempts) {
    ++attempts;
    __LOG0(LCOL01 " nqptp data not mapped attempts={}\n", module_id, "CLOCK_MAPPED", attempts);
  }

  return ok;
}

bool MasterClock::mapSharedMem() {
  int prev_state;

  if (isMapped() == false) {
    // prevent thread cancellation while mapping
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &prev_state);

    auto shm_fd = shm_open(shm_name.c_str(), O_RDWR, 0);

    if (shm_fd >= 0) {
      // flags must be PROT_READ | PROT_WRITE to allow writes
      // to mapped memory for the mutex
      constexpr auto flags = PROT_READ | PROT_WRITE;
      constexpr auto bytes = sizeof(nqptp);

      mapped = mmap(NULL, bytes, flags, MAP_SHARED, shm_fd, 0);

      close(shm_fd); // done with the shm memory fd

      __LOG0(LCOL01 " complete={}\n", module_id, "CLOCK_MAPPING", isMapped());
    } else {
      __LOG0(LCOL01 " clock shm_open failed, error={}\n", module_id, "FATAL", errno);
    }

    pthread_setcancelstate(prev_state, nullptr);
  }

  return isMapped();
}

void MasterClock::peersUpdate(const Peers &new_peers) {
  __LOGX(LCOL01 " count={}\n", module_id, csv("NEW PEERS"), new_peers.size());

  // queue the update to prevent collisions
  local_strand.post( //
      [this, peers = std::move(new_peers)]() {
        // build the msg: "<shm_name> T <ip_addr> <ip_addr>" + null terminator
        uint8v msg;
        auto w = std::back_inserter(msg);

        fmt::format_to(w, "{} T", shm_name);

        if (peers.empty() == false) {
          fmt::format_to(w, " {}", fmt::join(peers, " "));
        }

        msg.push_back(0x00); // must be null terminated

        __LOGX(LCOL01 " peers={}\n", module_id, csv("PEERS UPDATE"), msg.view());

        error_code ec;
        auto bytes = socket.send_to(asio::buffer(msg), remote_endpoint, 0, ec);

        if (ec) {
          __LOG0(LCOL01 " send msg failed, reason={} bytes={}\n", //
                 module_id, "PEERS_UPDATE", ec.message(), bytes);
        }
      });
}

void MasterClock::unMap() {
  if (isMapped()) {
    munmap(mapped, sizeof(nqptp));

    mapped = nullptr;
  }

  [[maybe_unused]] error_code ec;
  socket.close(ec);
}

} // namespace pierre
