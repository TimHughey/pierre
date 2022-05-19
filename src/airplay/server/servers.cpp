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

#include "server/servers.hpp"
#include "conn_info/conn_info.hpp"
#include "server/audio.hpp"
#include "server/control.hpp"
#include "server/event.hpp"
#include "server/rtsp.hpp"

#include <array>

namespace pierre {
namespace airplay {

namespace shared {
std::optional<shServers> __servers;
std::optional<shServers> &servers() { return __servers; }
} // namespace shared

Servers::ServerPtr Servers::fetch(ServerType type) {
  if (map.contains(type) == false) {
    return ServerPtr(nullptr);
  }

  return map.at(type);
}

Port Servers::localPort(ServerType type) {
  auto svr = fetch(type);

  if (svr.get() == nullptr) {
    switch (type) {
      case ServerType::Audio:
        map.emplace(type, new server::Audio(di));
        break;

      case ServerType::Event:
        map.emplace(type, new server::Event(di));
        break;

      case ServerType::Control:
        map.emplace(type, new server::Control(di));
        break;

      case ServerType::Rtsp:
        map.emplace(type, new server::Rtsp(di));
        break;
    }

    svr = map.at(type);

    svr->asyncLoop(); // start the server
  }

  return svr->localPort();
}

void Servers::teardown() {
  using enum ServerType;

  for (const auto type : std::array{Audio, Event, Control, Rtsp}) {
    teardown(type);
  }
}

void Servers::teardown(ServerType type) {
  auto svr = fetch(type);

  if (svr.get() != nullptr) {
    svr->teardown();
    map.erase(type);
  }
}

/*

TeardownBarrier Servers::teardown(TeardownPhase phase) {
  teardown_phase = phase; // save phase for when teardown is executed

  // be certain the previous teardown is answered (if there was one)
  // teardown_request.set_value(TeardownPhase::None);

  teardown_request = Teardown(); // create a new promise

  if (true) {
    constexpr auto f = FMT_STRING("{} {} phase={}\n");
    fmt::print(f, runTicks(), fnName(), phase);
  }

  ConnInfo::ptr()->airplay_gid.clear();
  ConnInfo::ptr()->dacp_active_remote.clear();

  return teardown_request.get_future();
}

void Servers::teardownFinished() {
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

*/
} // namespace airplay
} // namespace pierre
