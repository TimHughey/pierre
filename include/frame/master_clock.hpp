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

#include "base/input_info.hpp"
#include "base/pet.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "frame/clock_info.hpp"
#include "io/io.hpp"
#include "lcs/logger.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <future>
#include <latch>
#include <memory>
#include <pthread.h>
#include <sys/mman.h>
#include <vector>

namespace pierre {

class MasterClock {
private:
  static constexpr auto LOCALHOST{"127.0.0.1"};
  static constexpr uint16_t CTRL_PORT{9000}; // see note
  static constexpr uint16_t NQPTP_VERSION{8};

public:
  struct nqptp {
    pthread_mutex_t copy_mutes;           // for safely accessing the structure
    uint16_t version;                     // check version==VERSION
    uint64_t master_clock_id;             // the current master clock
    char master_clock_ip[64];             // where it's coming from
    uint64_t local_time;                  // the time when the offset was calculated
    uint64_t local_to_master_time_offset; // add this to the local time to get master clock time
    uint64_t master_clock_start_time;     // this is when the master clock became master}
  };

public:
  MasterClock() noexcept;
  ~MasterClock() noexcept;

  /// @brief Get ClockInfo via a shared future. The minimum time allowed between requests is
  ///        configured via the external .toml file.  If called before the minimum time
  ///        has elpased this function will immediately set the future with an empty
  ///        ClockInfo object.
  /// @return std::shared_future<ClockInfo>
  clock_info_future info() noexcept { // static
    auto prom = std::make_shared<std::promise<ClockInfo>>();

    auto clock_info = load_info_from_mapped();

    if (clock_info.ok()) {
      // clock info is good, immediately set the future
      prom->set_value(clock_info);
    } else {
      // perform the retry on the MasterClock io_ctx enabling caller to continue other work
      // while the clock becomes useable and the future is set to ready

      auto timer = std::make_unique<steady_timer>(io_ctx);
      // get a pointer to the timer since we move the timer into the async_wait
      auto t = timer.get();

      t->expires_after(InputInfo::lead_time_min);
      t->async_wait([this, timer = std::move(timer), prom = prom](error_code ec) {
        // if success get and return ClockInfo (could be ok() == false)
        if (!ec) {
          prom->set_value(std::move(load_info_from_mapped()));
        } else {
          prom->set_value(ClockInfo());
        }
      });
    }

    return prom->get_future().share();
  }

  void peers(const Peers &peer_list) noexcept { peers_update(peer_list); }
  void peers_reset() noexcept { peers_update(Peers()); }

  // misc debug
  void dump();

private:
  bool is_mapped() const {
    static uint32_t attempts = 0;
    auto ok = (mapped && (mapped != MAP_FAILED));

    if (!ok && attempts) {
      ++attempts;
      INFO(module_id, "mapped", "nqptp data not mapped attempts={}\n", attempts);
    }

    return ok;
  }

  const ClockInfo load_info_from_mapped();
  bool map_shm();

  void peers_update(const Peers &peers);

  bool ready() noexcept {
    const auto &clock_info = load_info_from_mapped();

    return clock_info.ok() && clock_info.master_for_at_least(333ms);
  }

private:
  // order dependent
  io_context io_ctx;
  work_guard guard;
  udp_socket socket;
  udp_endpoint remote_endpoint;
  const string shm_name; // shared memmory segment name (built by constructor)
  const int thread_count;
  std::shared_ptr<std::latch> shutdown_latch;

  // order independent
  void *mapped{nullptr}; // mmapped region of nqptp data struct

public:
  static constexpr csv module_id{"master_clock"};
};

/*  The control port expects a UDP packet with the first space-delimited string
    being the name of the shared memory interface (SMI) to be used.
    This allows client applications to have a dedicated named SMI interface
    with a timing peer list independent of other clients. The name given must
    be a valid SMI name and must contain no spaces. If the named SMI interface
    doesn't exist it will be created by NQPTP. The SMI name should be delimited
    by a space and followed by a command letter. At present, the only command
    is "T", which must followed by nothing or by a space and a space-delimited
    list of IPv4 or IPv6 numbers, the whole not to exceed 4096 characters in
    total. The IPs, if provided, will become the new list of timing peers,
    replacing any previous list. If the master clock of the new list is the
    same as that of the old list, the master clock is retained without
    resynchronisation; this means that non-master devices can be added and
    removed without disturbing the SMI's existing master clock. If no timing
    list is provided, the existing timing list is deleted. (In future version
    of NQPTP the SMI interface may also be deleted at this point.) SMI
    interfaces are not currently deleted or garbage collected. */

} // namespace pierre
