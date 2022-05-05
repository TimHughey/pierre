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

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <thread>

#include "anchor/time/shm/struct.hpp"
#include "core/host.hpp"

namespace pierre {
namespace anchor {

using namespace std::chrono;
using namespace std::chrono_literals;
using namespace std::string_literals;

using jthread = std::jthread;
using mutex = std::mutex;
using unique_lock = std::unique_lock<mutex>;
using cond_var = std::condition_variable_any;
using runtime_error = std::runtime_error;
using string = std::string;
using string_view = std::string_view;
using src_loc = std::source_location;
using SteadyClock = std::chrono::steady_clock;

typedef std::vector<string> Peers;
typedef const char *ccs;
typedef const string_view csv;
typedef const src_loc csrc_loc;
typedef uint64_t ClockId;

struct ClockInfo {
  ClockId clockID = 0;              // current master clock
  MasterClockIp masterClockIp{0};   // IP of master clock
  uint64_t sampleTime = 0;          // time when the offset was calculated
  uint64_t rawOffset = 0;           // master clock time = sampleTime + rawOffset
  uint64_t mastershipStartTime = 0; // when the master clock became maste

  bool ok(auto age = 10s) const {
    const auto old_ns = SteadyClock::duration(age).count();
    return ((int64_t)sampleTime < ((int64_t)now() - (int64_t)old_ns) ? true : false);
  }

  static uint64_t now() { return SteadyClock::now().time_since_epoch().count(); }

  void dump() const {
    const auto now_ns = now();

    const int64_t now_minus_sample_time = (int64_t)now_ns - (int64_t)sampleTime;

    const auto hex_fmt_str = FMT_STRING("{:>35}={:#x}\n");
    const auto dec_fmt_str = FMT_STRING("{:>35}={}\n");

    fmt::print("{}\n", fnName());
    fmt::print(hex_fmt_str, "clockId", clockID);
    fmt::print(dec_fmt_str, "now", now_ns);
    fmt::print(dec_fmt_str, "mastershipStart", mastershipStartTime);
    fmt::print(dec_fmt_str, "rawOffset", rawOffset);
    fmt::print(dec_fmt_str, "sampleTime", sampleTime);
    fmt::print(dec_fmt_str, "now - sampleTime", now_minus_sample_time);
    fmt::print("\n");
  }

  ccs fnName(csrc_loc loc = src_loc::current()) const { return loc.function_name(); }
};

class Clock {
private:
  static constexpr auto THREAD_NAME = "Clock";
  static constexpr auto LOCALHOST = "127.0.0.1";

public:
  Clock(shHost host) {
    mtx_ready.lock();
    shm_name = fmt::format("/{}-{}", host->serviceName(), host->deviceID()); // make shm_name

    __init();
  }

  // prevent copies, moves and assignments
  Clock(const Clock &clock) = delete;            // no copy
  Clock(const Clock &&clock) = delete;           // no move
  Clock &operator=(const Clock &clock) = delete; // no copy assignment
  Clock &operator=(Clock &&clock) = delete;      // no move assignment

  const ClockInfo info();
  bool isMapped(bool throw_if_not = true, csrc_loc loc = src_loc::current()) const;

  static uint64_t now() { return SteadyClock::now().time_since_epoch().count(); }

  void peersReset() { peersUpdate(Peers()); }
  void peers(const Peers &peer_list) { peersUpdate(peer_list); }

  [[nodiscard]] bool refresh(); // refresh cached values
  void teardown() { peersReset(); }

  // misc debug
  void dump() const;
  ccs fnName(csrc_loc loc = src_loc::current()) const { return loc.function_name(); }

private:
  void __init();

  void openAndMap();
  void runLoop();
  void unMap();

  void peersUpdate(const Peers &peers);

private:
  string shm_name;        // shared memmory segment name (built by constructor)
  jthread thr;            // other thread
  void *mapped = nullptr; // mmapped region of nqptp data struct
  string peer_list;       // timing peers (update when not empty)
  mutex mtx_ready;        // locked until mapped and thread startup
  mutex mtx_peers;        // mtx to protect updates to peers
  mutex mtx_wakeup;       // mtx for cv_send_msg
  cond_var wakeup;        // cond var to trigger ctrl msg send via client
};

} // namespace anchor
} // namespace pierre