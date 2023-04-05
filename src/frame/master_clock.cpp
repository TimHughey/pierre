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
#include "base/pet.hpp"
#include "base/thread_util.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"

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
  static constexpr csv fn_id{"refresh"};
  auto rc = true;

  int prev_state; // to restore pthread cancel state

  if (is_mapped() == false) {
    // prevent thread cancellation while mapping
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &prev_state);

    if (auto shm_fd = shm_open(shm_name.data(), O_RDWR, 0); shm_fd >= 0) {
      // flags must be PROT_READ | PROT_WRITE to allow writes to mapped memory for the mutex
      constexpr auto flags = PROT_READ | PROT_WRITE;
      mapped = mmap(NULL, len(), flags, MAP_SHARED, shm_fd, 0);

      close(shm_fd); // done with the shm memory fd

      INFO(MasterClock::module_id, fn_id, "complete={}\n", is_mapped());
    } else {
      INFO(MasterClock::module_id, fn_id, "clock map shm={} error={}\n", shm_name, strerror(errno));
    }

    pthread_setcancelstate(prev_state, nullptr);
  }

  if (is_mapped()) {
    // prevent thread cancellation while copying
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &prev_state);

    // shm_structure embeds a mutex at the beginning that
    // we use to guard against data races
    auto *mtx = static_cast<pthread_mutex_t *>(mapped);
    pthread_mutex_lock(mtx);

    memcpy(&latest, (char *)mapped, len());
    if (latest.version != NQPTP_VERSION) {
      INFO(MasterClock::module_id, fn_id, "nqptp version mismatch vsn={}", latest.version);

      rc = false;
    }

    // release the mutex
    pthread_mutex_unlock(mtx);
    pthread_setcancelstate(prev_state, nullptr);
  }

  return std::make_pair(rc && is_mapped(), &latest);
}

inline void unmap() noexcept {
  if (is_mapped()) munmap(std::exchange(mapped, nullptr), len());
}

} // namespace nqptp

// create the MasterClock
MasterClock::MasterClock(asio::thread_pool &thread_pool) noexcept
    : shm_name(config_val<MasterClock, string>("shm_name", "/nqptp")),
      nqptp_addr(asio::ip::make_address(LOCALHOST)), // where nqptp is running
      nqptp_rep(nqptp_addr, CTRL_PORT),              // endpoint of nqptp daemon
      local_strand(asio::make_strand(thread_pool)),  // serialize peer changes
      peer(local_strand, ip_udp::v4())               // construct and open
{

  INFO_INIT("sizeof={:>5} shm_name={} dest={}:{} nqptp_len={}\n", sizeof(MasterClock), shm_name,
            nqptp_rep.address().to_string(), nqptp_rep.port(), nqptp::len());

  peers(); // reset the peers (creates the shm name)}
}

MasterClock::~MasterClock() noexcept {
  static constexpr csv fn_id("shutdown");

  if (peer.is_open()) {
    // add peers reset logic
  }

  nqptp::unmap();
}

// NOTE: new data is available every 126ms
const ClockInfo MasterClock::load_info_from_mapped() noexcept {
  auto [rc, nd] = nqptp::refresh(shm_name);

  if (!rc) return ClockInfo();

  return ClockInfo(nqptp::mclock_id(nd), nqptp::mclock_ip(nd),
                   nqptp::local_time(nd),                  // aka sample time
                   nqptp::local_to_master_time_offset(nd), // aka raw offset
                   nqptp::mclock_start_time(nd));
}

void MasterClock::peers(const Peers &&new_peers) noexcept {
  static constexpr csv fn_id{"peers_update"};
  INFO_AUTO("new peers count={}\n", new_peers.size());

  // queue the update to prevent collisions
  asio::post(local_strand, [this, peers = std::move(new_peers)]() {
    // build the msg: "<shm_name> T <ip_addr> <ip_addr>" + null terminator
    uint8v msg;
    auto w = std::back_inserter(msg);

    fmt::format_to(w, "{} T", shm_name);

    if (peers.size()) fmt::format_to(w, " {}", fmt::join(peers, " "));

    msg.push_back(0x00); // must be null terminated

    INFO_AUTO("peers msg={}\n", msg.view());

    error_code ec;
    auto bytes = peer.send_to(asio::buffer(msg), nqptp_rep, 0, ec);

    if (ec) INFO_AUTO("send msg failed, reason={} bytes={}\n", ec.message(), bytes);
  });
}

// misc debug
void MasterClock::dump() {
  static constexpr csv fn_id{"dump"};
  INFO_AUTO("\n{}\n", load_info_from_mapped().inspect());
}

} // namespace pierre
