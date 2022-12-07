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
#include "base/io.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "config/config.hpp"

#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <iterator>
#include <latch>
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
MasterClock::MasterClock() noexcept
    : guard(io_ctx.get_executor()),                                  // syncronize clock io
      socket(io_ctx, ip_udp::v4()),                                  // construct and open
      remote_endpoint(asio::ip::make_address(LOCALHOST), CTRL_PORT), // nqptp endpoint
      shm_name(Config()                                              // make shm_name
                   .at("frame.clock.shm_name"sv)
                   .value_or<string>("/nqptp")) {

  INFO(module_id, "CONSTRUCT", "shm_name={} dest={}:{}\n", //
       shm_name, remote_endpoint.address().to_string(), remote_endpoint.port());
}

clock_info_future MasterClock::info() noexcept { // static
  auto prom = std::make_shared<std::promise<ClockInfo>>();
  auto clock_info = shared::master_clock->load_info_from_mapped();

  if (clock_info.ok()) {
    // clock info is good, immediately set the future
    prom->set_value(clock_info);
  } else {
    // clock info not ready yet, retry
    shared::master_clock->info_retry(prom);
  }

  return prom->get_future();
}

void MasterClock::info_retry(clock_info_prom prom,      //
                             Nanos max_wait, Elapsed e, //
                             steady_timer_ptr timer) noexcept {

  // perform the retries on the MasterClock io_ctx enabling caller to continue other work
  // while the clock becomes useable and the future is set to ready

  auto clock_info = load_info_from_mapped();

  if (clock_info.ok()) { // it's good, set the prom and return
    prom->set_value(clock_info);
  } else {
    // clock info still not ready prepare for a retry

    // this might be the first time through...  prepare retry support objects
    if (max_wait == Nanos::zero()) {
      e.reset(); // start timing how long we've been retrying
      const int max_frames = Config().at("frame.clock.info.max_wait_frames"sv).value_or(10);
      max_wait = InputInfo::lead_time * max_frames;
    }

    if (!timer) timer = std::make_shared<steady_timer>(io_ctx);

    // ok, we have our retry support objects

    timer->expires_after(InputInfo::lead_time);
    timer->async_wait([=, this](const error_code ec) mutable {
      if (!ec && (e < max_wait)) {
        info_retry(prom, max_wait, e, timer);
      } else {
        // error or max_wait exceeded, give up
        INFO(module_id, "INFO_RETRY", "no clock info after {}\n", e.humanize());
        prom->set_value(clock_info);
      }
    });
  }
}

void MasterClock::init() {
  if (shared::master_clock.has_value() == false) {
    shared::master_clock.emplace().init_self();
  }
}

void MasterClock::init_self() noexcept {
  std::latch latch{THREAD_COUNT};

  for (auto n = 0; n < THREAD_COUNT; n++) {
    // notes:
    //  1. start DSP processing threads
    threads.emplace_back([=, this, &latch](std::stop_token token) mutable {
      tokens.add(std::move(token));

      name_thread("Master Clock", n);
      latch.arrive_and_wait();
      io_ctx.run();
    });
  }

  latch.wait();  // caller waits until all threads are started
  peers_reset(); // reset the peers (and creates the shm mem name)
}

bool MasterClock::is_mapped() const {
  static uint32_t attempts = 0;
  auto ok = (mapped && (mapped != MAP_FAILED));

  if (!ok && attempts) {
    ++attempts;
    INFO(module_id, "CLOCK_MAPPED", "nqptp data not mapped attempts={}\n", attempts);
  }

  return ok;
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
      // flags must be PROT_READ | PROT_WRITE to allow writes
      // to mapped memory for the mutex
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
  INFOX(module_id, "NEW PEERS", "count={}\n", new_peers.size());

  // queue the update to prevent collisions
  asio::post( //
      io_ctx, //
      [this, peers = std::move(new_peers)]() {
        // build the msg: "<shm_name> T <ip_addr> <ip_addr>" + null terminator
        uint8v msg;
        auto w = std::back_inserter(msg);

        fmt::format_to(w, "{} T", shm_name);

        if (peers.empty() == false) {
          fmt::format_to(w, " {}", fmt::join(peers, " "));
        }

        msg.push_back(0x00); // must be null terminated

        INFOX(module_id, "PEERS UPDATE", "peers={}\n", msg.view());

        error_code ec;
        auto bytes = socket.send_to(asio::buffer(msg), remote_endpoint, 0, ec);

        if (ec) {
          INFO(module_id, "PEERS_UPDATE", "send msg failed, reason={} bytes={}\n", //
               ec.message(), bytes);
        }
      });
}

bool MasterClock::ready() { // static
  const auto &clock_info = shared::master_clock->load_info_from_mapped();

  return clock_info.ok() && clock_info.master_for_at_least(333ms);
}

void MasterClock::reset() { // static
  shared::master_clock->unMap();
  shared::master_clock.reset();
}

void MasterClock::unMap() {
  if (is_mapped()) {
    munmap(mapped, sizeof(nqptp));

    mapped = nullptr;
  }

  [[maybe_unused]] error_code ec;
  socket.close(ec);
}

// misc debug
void MasterClock::dump() {
  INFO(module_id, "DUMP", "inspect info\n{}\n", load_info_from_mapped().inspect());
}

} // namespace pierre
