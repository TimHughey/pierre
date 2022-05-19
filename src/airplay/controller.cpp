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

#include "controller.hpp"
#include "anchor/anchor.hpp"
#include "clock/clock.hpp"
#include "common/ss_inject.hpp"
#include "common/typedefs.hpp"
#include "conn_info/conn_info.hpp"
#include "core/features.hpp"
#include "server/servers.hpp"

#include <chrono>
#include <list>
#include <optional>
#include <thread>

using namespace std::literals::chrono_literals;

namespace pierre {
namespace airplay {

namespace shared {
std::optional<shController> __controller;
std::optional<shController> &controller() { return shared::__controller; }
} // namespace shared

namespace errc = boost::system::errc;

// executed once at startup (by one of the threads) to create necessary
// resources (e.g. Clock, Anchor, Servers
void Controller::kickstart() {
  if (true) { // debug
    Features features;
    constexpr auto f = FMT_STRING("{} {} kickstart features={:x}\n");
    fmt::print(f, runTicks(), fnName(), features.ap2_default());
  }

  auto clock_inject = clock::Inject{.io_ctx = io_ctx,
                                    .service_name = Host::ptr()->serviceName(),
                                    .device_id = Host::ptr()->deviceID()};

  auto &clock = opt_clock.emplace(clock_inject);
  clock.peersReset();

  Anchor::init(clock);
  ConnInfo::init();

  Servers::init({.io_ctx = io_ctx});

  // finally, start listening for Rtsp messages
  Servers::ptr()->localPort(ServerType::Rtsp);
}

void Controller::run() {
  nameThread(0); // controller thread is Airplay 00

  for (auto n = 1; n < MAX_THREADS(); n++) {
    // notes:
    //  1. all threads run the same io_ctx

    if (n == 1) { // thread 1 handles start-up (then general purpose)
      threads.emplace_back([n = n, self = shared_from_this()] {
        self->watchDog(); // watchDog() ensures io_ctx has work
        self->nameThread(n);
        self->io_ctx.run(); // run io_ctx for startup activities
      });
    }

    if (n > 1) { // start rest of threads
      threads.emplace_back([n = n, self = shared_from_this()] {
        std::call_once(self->_kickstart_, [self] { self->kickstart(); }); // sync on call_once
        self->nameThread(n);
        self->io_ctx.run(); // one of these threads
      });
    }
  }

  io_ctx.run(); // run io_ctx on controller thread
}

Thread &Controller::start() {
  airplay_thread = Thread(&Controller::run, this);

  return airplay_thread;
}

void Controller::watchDog() {
  // cancels any running timers
  [[maybe_unused]] auto timers_cancelled = watchdog_timer.expires_after(250ms);

  watchdog_timer.async_wait([self = shared_from_this()](const error_code ec) {
    if (ec != errc::success) { // any error, fall out of scope
      constexpr auto f = FMT_STRING("{} {} going out of scope reason={}\n");
      fmt::print(f, runTicks(), fnName(), ec.what());
      return;
    }

    if (false) { // debug
      constexpr auto f = FMT_STRING("{} {}\n");
      fmt::print(f, runTicks(), fnName());
    }

    auto stop_token = self->airplay_thread.get_stop_token();

    if (stop_token.stop_requested()) { // thread wants to stop
      self->io_ctx.stop();             // stop the io_ctx (stops other threads)
      return;                          // fall out of scope
    }

    self->watchDog(); // restart the timer (keeps us in scope)
  });
}

} // namespace airplay
} // namespace pierre
