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
#include "base/host.hpp"
#include "base/logger.hpp"
#include "base/pet.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"

#include <algorithm>
#include <cstdlib>
#include <errno.h>
#include <exception>
#include <fcntl.h>
#include <iterator>
#include <latch>
#include <pthread.h>
#include <ranges>
#include <sys/mman.h>
#include <thread>
#include <tuple>
#include <utility>

namespace pierre {

namespace nqptp {

struct data {
  pthread_mutex_t copy_mtx;             // for safely accessing the structure
  uint16_t version;                     // check version==VERSION
  uint64_t master_clock_id;             // the current master clock
  char master_clock_ip[64];             // where it's coming from
  uint64_t local_time;                  // the time when the offset was calculated
  uint64_t local_to_master_time_offset; // add this to the local time to get master clock time
  uint64_t master_clock_start_time;     // this is when the master clock became master}
};

static constexpr uint16_t NQPTP_VERSION{8};
static void *mapped{nullptr};
static data latest;

inline constexpr const auto mclock_id(auto *l) noexcept { return l->master_clock_id; }
inline const auto mclock_start_time(auto *l) noexcept {
  return pet::from_val<Nanos>(l->master_clock_start_time);
}

inline bool is_mapped() noexcept { return mapped && (mapped != MAP_FAILED); }

inline constexpr size_t len() noexcept { return sizeof(latest); }
inline constexpr const auto local_time(auto *l) noexcept { return l->local_time; }
inline constexpr const auto local_to_master_time_offset(auto *l) noexcept {
  return l->local_to_master_time_offset;
}

inline constexpr string mclock_ip(auto *l) noexcept { return string(l->master_clock_ip); }

inline std::pair<bool, data *> refresh(const auto &shm_name) noexcept {
  INFO_MODULE_ID(MasterClock::module_id);
  INFO_AUTO_CAT("refresh");

  auto rc = true;

  // once only to perform initial mapping
  if (is_mapped() == false) {
    if (auto shm_fd = shm_open(shm_name.data(), O_RDWR, 0); shm_fd >= 0) {
      // flags must be PROT_READ | PROT_WRITE to allow writes to mapped memory for the mutex

      int prev_state; // to restore pthread cancel state
                      // prevent thread cancellation while mapping
      pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &prev_state);
      constexpr auto flags = PROT_READ | PROT_WRITE;
      mapped = mmap(NULL, len(), flags, MAP_SHARED, shm_fd, 0);

      close(shm_fd); // done with the shm memory fd

      pthread_setcancelstate(prev_state, nullptr);

      INFO_AUTO("mapped={}\n", is_mapped());
    } else {
      INFO_AUTO("clock map shm={} error={}\n", shm_name, strerror(errno));
    }
  }

  if (is_mapped()) {
    // shm_structure embeds a mutex at the beginning that
    // we use to guard against data races
    auto *mtx = static_cast<pthread_mutex_t *>(mapped);
    pthread_mutex_lock(mtx);

    memcpy(&latest, (char *)mapped, len());
    if (latest.version != NQPTP_VERSION) {
      INFO_AUTO("nqptp version mismatch vsn={}", latest.version);

      rc = false;
    }

    // release the mutex
    pthread_mutex_unlock(mtx);
  }

  return std::make_pair(rc && is_mapped(), &latest);
}

inline void unmap() noexcept {
  if (is_mapped()) munmap(std::exchange(mapped, nullptr), len());
}

} // namespace nqptp

// create the MasterClock
MasterClock::MasterClock(asio::io_context &io_ctx) noexcept
    : nqptp_addr(asio::ip::make_address(LOCALHOST)), // where nqptp is running
      nqptp_rep(nqptp_addr, CTRL_PORT),              // endpoint of nqptp daemon
      peer(io_ctx, ip_udp::v4())                     // construct and open
{
  INFO_INIT("sizeof={:>5} shm_name={} dest={}:{} nqptp_len={}\n", sizeof(MasterClock), SHM_NAME,
            nqptp_rep.address().to_string(), nqptp_rep.port(), nqptp::len());

  peers(Peers()); // reset the peers (creates the shm name)}
}

MasterClock::~MasterClock() noexcept {
  if (peer.is_open()) {
    [[maybe_unused]] error_code ec;
    peer.close(ec);
  }

  nqptp::unmap();
}

// NOTE: new data is available every 126ms
bool MasterClock::info(ClockInfo &info) noexcept {

  if (auto refresh = nqptp::refresh(SHM_NAME); refresh.first) {
    auto nd = refresh.second;

    info.clock_id = nqptp::mclock_id(nd);
    info.masterClockIp = nqptp::mclock_ip(nd);
    info.sampleTime = nqptp::local_time(nd);
    info.rawOffset = nqptp::local_to_master_time_offset(nd);
    info.mastershipStartTime = nqptp::mclock_start_time(nd);
    info.sample_age.reset();
  } else {
    info = std::move(ClockInfo());
  }

  return info.ok();
}

const ClockInfo MasterClock::info() noexcept {

  auto [ok, nd] = nqptp::refresh(SHM_NAME);

  if (ok) {
    return ClockInfo(nqptp::mclock_id(nd), nqptp::mclock_ip(nd),
                     nqptp::local_time(nd),                  // aka sample time
                     nqptp::local_to_master_time_offset(nd), // aka raw offset
                     nqptp::mclock_start_time(nd));
  }

  return ClockInfo();
}

void MasterClock::peers(const Peers &&new_peers) noexcept {
  static constexpr csv fn_id{"peers_update"};
  INFO_AUTO("new peers count={}\n", new_peers.size());

  // queue the update to prevent collisions
  auto executor = peer.get_executor();
  asio::post(executor, [this, peers = std::move(new_peers)]() {
    // build the msg: "<shm_name> T <ip_addr> <ip_addr>" + null terminator
    uint8v msg;
    auto w = std::back_inserter(msg);

    fmt::format_to(w, "{} T", SHM_NAME);

    if (peers.size()) fmt::format_to(w, " {}", fmt::join(peers, " "));

    msg.push_back(0x00); // must be null terminated

    INFO_AUTO("peers msg={}\n", msg.view());

    error_code ec;
    auto bytes = peer.send_to(asio::buffer(msg), nqptp_rep, 0, ec);

    if (ec) INFO_AUTO("send msg failed, reason={} bytes={}\n", ec.message(), bytes);
  });
}

} // namespace pierre
