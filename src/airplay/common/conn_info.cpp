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
//
//  This work based on and inspired by
//  https://github.com/mikebrady/nqptp Copyright (c) 2021--2022 Mike Brady.

#include "common/conn_info.hpp"
#include "server/map.hpp"

#include <chrono>
#include <fmt/format.h>
#include <future>
#include <memory>
#include <string_view>

namespace pierre {
namespace airplay {

Port ConnInfo::localPort([[maybe_unused]] ServerType type) {
  // return the stored port
  return local_port_map.at(type);
}

void ConnInfo::saveActiveRemote(csv active_remote) { dacp_active_remote = active_remote; }

void ConnInfo::saveSessionKey(csr key) { session_key.assign(key.begin(), key.end()); }

void ConnInfo::saveStreamData(const StreamData &data) { stream_info = data; }

TeardownBarrier ConnInfo::teardown(TeardownPhase phase) {
  teardown_phase = phase; // save phase for when teardown is executed

  // be certain the previous teardown is answered (if there was one)
  // teardown_request.set_value(TeardownPhase::None);

  teardown_request = Teardown(); // create a new promise

  if (true) {
    constexpr auto f = FMT_STRING("{} {} phase={}\n");
    fmt::print(f, runTicks(), fnName(), phase);
  }

  airplay_gid.clear();
  dacp_active_remote.clear();

  return teardown_request.get_future();
}

void ConnInfo::teardownFinished() {
  if (true) { // log before removing the barrier
    fmt::print(FMT_STRING("{} {}\n"), runTicks(), fnName());
  }

  teardown_request.set_value(teardown_phase);
  teardown_phase = TeardownPhase::None;
}

bool ConnInfo::teardownIfNeeded() {
  auto full_teardown = false;

  // is there a promise waitng?
  if (teardown_phase != TeardownPhase::None) {
    if (true) { // debug
      constexpr auto f = FMT_STRING("{} {} requested={}\n");
      fmt::print(f, runTicks(), fnName(), teardown_phase);
    }

    // what promise did we make? Phase 1 or 2?
    switch (teardown_phase) {
      case TeardownPhase::None:
        // nothing to do
        break;

      case TeardownPhase::One:
        session_key.clear();
        stream_info.teardown();

        teardownFinished();
        break;

      case TeardownPhase::Two:
        // this is the big phase, setting the value and clearing promise
        // return full teardown to caller
        teardownFinished();
        full_teardown = true;
        break;
    }
  }

  return full_teardown;
}

} // namespace airplay
} // namespace pierre