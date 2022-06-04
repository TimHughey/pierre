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

#include "rtp_time/clock.hpp"
#include "packet/basic.hpp"
#include "rtp_time/shm/struct.hpp"

#include <algorithm>
#include <fcntl.h>
#include <fmt/format.h>
#include <iterator>
#include <pthread.h>
#include <ranges>
#include <sys/mman.h>
#include <utility>

namespace pierre {

namespace shared {
std::optional<shMasterClock> __master_clock;
std::optional<shMasterClock> &master_clock() { return __master_clock; }
} // namespace shared

namespace rtp_time {

bool Info::ok() const {
  if (clockID == 0) { // no master clock
    constexpr auto f = FMT_STRING("{} {} no clock id\n");
    fmt::print(f, runTicks(), fnName());
    return false;
  }

  return true;
}

bool Info::tooOld() const {
  if (ok() == false) {
    return false;
  }

  const auto now = nowNanos();

  auto print_msg = [&](csv reason) {
    constexpr auto f = FMT_STRING("{} {} {} sampleTime={} now={}\n");
    fmt::print(f, runTicks(), fnName(), reason, sampleTime.count(), now.count());
  };

  const auto too_old = now - AGE_MAX;
  if (sampleTime >= too_old) {
    print_msg("too old");
    return false;
  }

  return true;
}

bool Info::tooYoung() const {
  if (ok() == false) {
    return true;
  }

  const auto now = nowNanos();

  auto print_msg = [&](csv reason) {
    constexpr auto f = FMT_STRING("{} {} {} sampleTime={} now={}\n");
    fmt::print(f, runTicks(), fnName(), reason, sampleTime.count(), now.count());
  };

  const auto too_young = now - AGE_MIN;
  if (sampleTime <= too_young) {
    print_msg("too young");
    return true;
  }

  return false;
}

// misc Info debug
void Info::dump() const {
  const auto now = nowNanos();

  const auto hex_fmt_str = FMT_STRING("{:>35}={:#x}\n");
  const auto dec_fmt_str = FMT_STRING("{:>35}={}\n");
  [[maybe_unused]] const auto flt_fmt_str = FMT_STRING("{:>35}={:>+.3}\n");

  fmt::print("{} {}\n", runTicks(), fnName());
  fmt::print(hex_fmt_str, "clockId", clockID);
  fmt::print(dec_fmt_str, "rawOffset", (int64_t)rawOffset);
  fmt::print(dec_fmt_str, "now_ns", now.count());
  fmt::print(dec_fmt_str, "mastershipStart", mastershipStartTime.count());
  fmt::print(dec_fmt_str, "sampleTime", sampleTime.count());

  rtp_time::Seconds master_for_secs = now - mastershipStartTime;
  fmt::print(flt_fmt_str, "master_for_secs", master_for_secs.count());

  rtp_time::Seconds sample_age_secs = sampleTime - now;
  fmt::print(flt_fmt_str, "sample_age_secs", sample_age_secs.count());

  fmt::print("\n");
}

} // namespace rtp_time

using namespace boost::asio;
using namespace boost::system;
using namespace pierre::packet;
namespace ranges = std::ranges;

shMasterClock MasterClock::init(const rtp_time::Inject &inject) {
  return shared::master_clock().emplace(new MasterClock(inject));
}

shMasterClock MasterClock::ptr() { return shared::master_clock().value()->shared_from_this(); }

void MasterClock::reset() { shared::master_clock().reset(); }

// create the MasterClock
MasterClock::MasterClock(const rtp_time::Inject &di)
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

const rtp_time::Info MasterClock::getInfo() {
  if (mapSharedMem() == false) {
    return rtp_time::Info();
  }

  int prev_state;
  rtp_time::shm::structure data;

  // prevent thread cancellation while copying
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &prev_state);

  // shm_structure embeds a mutex at the beginning
  pthread_mutex_t *mtx = (pthread_mutex_t *)mapped;

  // use the mutex to guard while reading
  pthread_mutex_lock(mtx);

  rtp_time::shm::copy(mapped, &data);
  // release the mutex
  pthread_mutex_unlock(mtx);
  pthread_setcancelstate(prev_state, nullptr);

  // find the clock IP string
  string_view clock_ip_sv = string_view(data.master_clock_ip, rtp_time::shm::sizeClockIp());
  auto trim_pos = clock_ip_sv.find_first_of('\0');
  clock_ip_sv.remove_suffix(clock_ip_sv.size() - trim_pos);

  auto info = rtp_time::Info // calculate a local view of the nqptp data
      {.clockID = data.master_clock_id,
       .masterClockIp = string(clock_ip_sv),
       .sampleTime = rtp_time::from_ns(data.local_time),
       .rawOffset = data.local_to_master_time_offset,
       .mastershipStartTime = rtp_time::from_ns(data.master_clock_start_time)};

  return info;
}

bool MasterClock::isMapped(csrc_loc loc) const {
  static uint32_t attempts = 0;
  auto ok = (mapped && (mapped != MAP_FAILED));

  if (!ok && attempts && true) { // debug
    constexpr auto f = FMT_STRING("{} {} nqptp data not mapped attempts={}\n");
    fmt::print(f, runTicks(), loc.function_name(), attempts);
    ++attempts;
  }

  return ok;
}

bool MasterClock::mapSharedMem() {
  int prev_state;

  // prevent thread cancellation while mapping
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &prev_state);

  if (isMapped() == false) {
    auto shm_fd = shm_open(shm_name.c_str(), O_RDWR, 0);

    if ((shm_fd < 0) && true) { // debug
      constexpr auto f = FMT_STRING("{} {} clock shm_open failed\n");
      fmt::print(f, runTicks(), fnName());
    }

    // flags must be PROT_READ | PROT_WRITE to allow writes
    // to mapped memory for the mutex
    constexpr auto flags = PROT_READ | PROT_WRITE;
    constexpr auto bytes = rtp_time::shm::size();

    mapped = mmap(NULL, bytes, flags, MAP_SHARED, shm_fd, 0);

    close(shm_fd); // done with the shm memory fd

    if (false) { // debug
      constexpr auto f = FMT_STRING("{} {} clock mapping complete={}\n");
      fmt::print(f, runTicks(), fnName(), isMapped());
    }
  }

  pthread_setcancelstate(prev_state, nullptr);

  return isMapped();
}

bool MasterClock::ok() { return getInfo().ok(); }

void MasterClock::peersUpdate(const rtp_time::Peers &new_peers) {
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

void MasterClock::unMap() {
  if (isMapped()) {
    munmap(mapped, rtp_time::shm::size());

    mapped = nullptr;
  }

  [[maybe_unused]] error_code ec;
  socket.shutdown(udp_socket::shutdown_both, ec);
  socket.close(ec);
}

} // namespace pierre
