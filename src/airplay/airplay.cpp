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

#include "airplay.hpp"
#include "clock/clock.hpp"
#include "common/anchor.hpp"
#include "common/conn_info.hpp"
#include "common/typedefs.hpp"
#include "server/map.hpp"

#include <chrono>
#include <list>
#include <optional>
#include <thread>

using namespace std::literals::chrono_literals;

namespace pierre {
namespace airplay {

// ok, let's just call this what it is... a singleton.  there will only be
// one AirPlay object for each run of Pierre.  as such, we take liberties with
// the use "global variables".  we don't use these variables globally, however.
// rather they are defined within the airplay namespace then injected as
// dependencies to the services / classes that need them.

// we make heavy use of optionals here
typedef std::optional<Thread> ThreadStorage;
typedef std::optional<server::Map> ServerMapStorage;
typedef std::optional<Clock> ClockStorage;
typedef std::optional<ConnInfo> ConnInfoStorage;
typedef std::optional<Anchor> AnchorStorage;
typedef std::optional<server::Inject> SSInjectStorage;

// actual storage of the objects
io_context __io_ctx__;           // one io_context will be run by multiple threads
ThreadStorage __thread__;        // the threads that run the io_context
ServerMapStorage __server_map__; // creates and keeps servers
ClockStorage __clock__;          // the nqptp clock (one instance per Airplay)
AnchorStorage __anchor__;        // depends on Clock (reusable per session)
ConnInfoStorage __conn__;        // info for the active connection
SSInjectStorage __ss_inject__;   // server/session dependency injection

} // namespace airplay

// now back to pierre namespace
using namespace airplay;

static void watchDog(auto &timer, const server::Inject &di) {
  timer.expires_after(250ms);

  timer.async_wait([&]([[maybe_unused]] error_code ec) {
    if (false) { // debug
      constexpr auto f = FMT_STRING("{} {}\n");
      fmt::print(f, runTicks(), fnName());
    }

    if (di.conn.teardownIfNeeded()) { // full teardown!
      di.io_ctx.stop();
    }

    watchDog(timer, di);
  });
}

// airplay workhorse method
//
// notes:
//  1. create dependencies that were not created during construction
//  2. this method holds the memory for those dependencies rather than the object
//  3. this approach:
//     - limits inclusion of airplay internal implementation details (*.hpp)
//
void Airplay::run() {
  // basic thread setup
  pthread_setname_np(pthread_self(), "Airplay");
  running = true;

  auto &io_ctx = airplay::__io_ctx__; // grab a reference to the io_ctx, for convenience
  high_res_timer timer(io_ctx);       // watchdog timer

  // grab references to the airplay injections, we need them to create various objects
  auto &host = di_storage->host;
  auto &service = di_storage->service;
  auto &mdns = di_storage->mdns;

  // create the clock
  auto &clock = __clock__.emplace( // note: optional knows the type, pass the args
      clock::Inject{
          .io_ctx = io_ctx,                   // Clock leverages the same io_ctx
          .service_name = host.serviceName(), // needed to create the shm_name
          .device_id = host.deviceID()        // needed to create the shm_name
      });

  auto &anchor = __anchor__.emplace(clock);   // create the anchor
  auto &conn = __conn__.emplace(*di_storage); // create conn info (no args)

  auto &ss_inject = __ss_inject__.emplace( // create the server/session injection
      server::Inject{
          .io_ctx = io_ctx,   // the common io_ctx
          .conn = conn,       // reusable connection info
          .anchor = anchor,   // reusable anchor info
          .host = host,       // host (some replies require info about the host)
          .service = service, // service (some replies need data from or update the service)
          .mdns = mdns        // mdns (some replies update mdns TXT info)
      });

  auto &server_map = __server_map__.emplace(ss_inject); // create the server / session map

  conn.storeLocalPortMap(server_map.portList());

  // prepare for actual startup by scheduling work with the io_ctx
  // NOTE:  the work is only SCHEDULED and will not be run until we call io_ctx.run() below
  server_map.localPort(ServerType::Rtsp); // returns local port and adds work to io_ctx

  watchDog(timer, ss_inject); // schedule the watch dog before starting the threads

  while (running) {
    for (auto n = 0; n < MAX_THREADS(); n++) {
      threads.emplace_back([&] {
        const auto handle = pthread_self();
        const auto name = fmt::format("AirPlay {:<02}", n);

        pthread_setname_np(handle, name.c_str());

        io_ctx.run();
      });
    }

    threads.clear();

    if (false) { // debug
      constexpr auto f = FMT_STRING("{} {}\n");
      fmt::print(f, runTicks(), fnName());
    }

    auto stop_token = __thread__->get_stop_token();

    if (stop_token.stop_requested()) {
      running = false;
    } else {
      io_ctx.reset();

      auto &server_map = __server_map__.emplace(ss_inject); // create the server / session map

      conn.storeLocalPortMap(server_map.portList());

      server_map.localPort(ServerType::Rtsp); // returns local port and adds work to io_ctx
      watchDog(timer, ss_inject);             // schedule the watch dog before starting the threads
    }
  }
}

std::jthread &Airplay::start(const Inject &di) {
  di_storage.emplace(di); // store a copy of the injected dependencies

  // if there happens to be an Airplay already running ask it to shutdown.
  // this is purely a safety net -- there will never be more than one Airplay
  // running per Pierre process
  if (__thread__.has_value()) {
    teardown(__thread__.value());
  }

  __thread__.emplace(Thread(&Airplay::run, &(*this)));

  return __thread__.value(); // return (move) the newly started thread to caller
}

} // namespace pierre
