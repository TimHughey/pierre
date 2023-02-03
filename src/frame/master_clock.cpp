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
#include "base/input_info.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "io/io.hpp"
#include "lcs/config.hpp"

#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <iterator>
#include <latch>
#include <ranges>
#include <thread>
#include <time.h>
#include <utility>

namespace pierre {

static auto cfg_path(csv key_path) noexcept {
  return toml::path(MasterClock::module_id).append(key_path);
}

static const auto cfg_shm_name() noexcept {
  return config()->at(cfg_path("shm_name")).value_or<string>("/nqptp");
}

static const auto cfg_threads() noexcept {
  return config()->at(cfg_path("threads")).value_or<int>(1);
}

// create the MasterClock
MasterClock::MasterClock() noexcept
    : guard(io_ctx.get_executor()),                                  // syncronize clock io
      socket(io_ctx, ip_udp::v4()),                                  // construct and open
      remote_endpoint(asio::ip::make_address(LOCALHOST), CTRL_PORT), // nqptp endpoint
      shm_name(cfg_shm_name()),                                      //
      thread_count(cfg_threads()),                                   //
      shutdown_latch(std::make_shared<std::latch>(thread_count))     //
{

  INFO(module_id, "init", "shm_name={} dest={}:{}\n", //
       shm_name,                                      //
       remote_endpoint.address().to_string(),         //
       remote_endpoint.port());

  auto latch = std::make_unique<std::latch>(thread_count);

  for (auto n = 0; n < thread_count; n++) {
    std::jthread([this, n = n, latch = latch.get(), shutdown_latch = shutdown_latch]() mutable {
      const auto thread_name = name_thread("clock", n);

      latch->arrive_and_wait();
      INFO(module_id, "init", "thread {}\n", thread_name);

      io_ctx.run();

      shutdown_latch->count_down();
      INFO(module_id, "shutdown", "thread {}\n", thread_name);
    }).detach();
  }

  latch->wait(); // caller waits until all threads are started

  peers(Peers()); // reset the peers (creates the shm name)}
}

MasterClock::~MasterClock() noexcept {
  static constexpr csv fn_id("shutdown");

  INFO(module_id, fn_id, "initiated\n");

  guard.reset();
  if (is_mapped()) munmap(mapped, sizeof(nqptp));

  try {
    socket.close();
  } catch (...) {
  }

  shutdown_latch->wait();

  INFO(module_id, fn_id, "complete, io_ctx stopped={}\n", io_ctx.stopped());
}

// NOTE: new data is available every 126ms
const ClockInfo MasterClock::load_info_from_mapped() {
  if (map_shm() == false) {
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
    INFO("{}\n", module_id, "FATAL", msg);
    throw(std::runtime_error(msg.c_str()));
  }

  // release the mutex
  pthread_mutex_unlock(mtx);
  pthread_setcancelstate(prev_state, nullptr);

  // find the clock IP string
  string_view clock_ip_sv = string_view(data.master_clock_ip, sizeof(nqptp::master_clock_ip));
  auto trim_pos = clock_ip_sv.find_first_of('\0');
  clock_ip_sv.remove_suffix(clock_ip_sv.size() - trim_pos);

  return ClockInfo(data.master_clock_id, string(clock_ip_sv),
                   data.local_time,                  // aka sample time
                   data.local_to_master_time_offset, // aka raw offset
                   pet::from_val<Nanos>(data.master_clock_start_time));
}

bool MasterClock::map_shm() {
  int prev_state;

  if (is_mapped() == false) {
    // prevent thread cancellation while mapping
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &prev_state);

    auto shm_fd = shm_open(shm_name.data(), O_RDWR, 0);

    if (shm_fd >= 0) {
      // flags must be PROT_READ | PROT_WRITE to allow writes to mapped memory for the mutex
      constexpr auto flags = PROT_READ | PROT_WRITE;
      constexpr auto bytes = sizeof(nqptp);

      mapped = mmap(NULL, bytes, flags, MAP_SHARED, shm_fd, 0);

      close(shm_fd); // done with the shm memory fd

      INFOX(module_id, "CLOCK_MAPPING", "complete={}\n", is_mapped());
    } else {
      INFO(module_id, "FATAL", "clock map failed shm={} error={}\n", shm_name, errno);
    }

    pthread_setcancelstate(prev_state, nullptr);
  }

  return is_mapped();
}

void MasterClock::peers_update(const Peers &new_peers) {
  static constexpr csv fn_id{"new_peers"};
  INFOX(module_id, fn_id, "count={}\n", new_peers.size());

  // queue the update to prevent collisions
  asio::post(io_ctx, [this, peers = std::move(new_peers)]() {
    // build the msg: "<shm_name> T <ip_addr> <ip_addr>" + null terminator
    uint8v msg;
    auto w = std::back_inserter(msg);

    fmt::format_to(w, "{} T", shm_name);

    if (peers.empty() == false) {
      fmt::format_to(w, " {}", fmt::join(peers, " "));
    }

    msg.push_back(0x00); // must be null terminated

    INFOX(module_id, fn_id, "peers={}\n", msg.view());

    error_code ec;
    auto bytes = socket.send_to(asio::buffer(msg), remote_endpoint, 0, ec);

    if (ec) INFO(module_id, fn_id, "send msg failed, reason={} bytes={}\n", ec.message(), bytes);
  });
}

// misc debug
void MasterClock::dump() {
  INFO(module_id, "DUMP", "inspect info\n{}\n", load_info_from_mapped().inspect());
}

} // namespace pierre
