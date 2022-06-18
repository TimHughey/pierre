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
#include "common/ss_inject.hpp"
#include "common/typedefs.hpp"
#include "conn_info/conn_info.hpp"
#include "core/features.hpp"
#include "core/host.hpp"
#include "player/player.hpp"
#include "rtp_time/anchor.hpp"
#include "rtp_time/clock.hpp"
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

shController Controller::init() { // static
  return shared::controller().emplace(new Controller());
}
shController Controller::ptr() { // wtatic
  return shared::controller().value()->shared_from_this();
}
void Controller::reset() { // static
  shared::controller().reset();
}

namespace errc = boost::system::errc;

// executed once at startup (by one of the threads) to create necessary
// resources (e.g. Clock, Anchor, Servers
void Controller::kickstart() {
  __LOG0("{} features={:#x}\n", moduleId, Features().ap2_default());

  watchDog();                       // watchDog() ensures io_ctx has work
  MasterClock::ptr()->peersReset(); // reset timing peers

  // finally, start listening for Rtsp messages
  Servers::ptr()->localPort(ServerType::Rtsp);
}

void Controller::run() {
  static std::once_flag once;
  nameThread(0); // controller thread is Airplay 00

  MasterClock::init({.io_ctx = io_ctx,
                     .service_name = Host::ptr()->serviceName(),
                     .device_id = Host::ptr()->deviceID()});

  Anchor::init();
  ConnInfo::init();
  Servers::init({.io_ctx = io_ctx});

  // run the controller with standard concurrency
  // other classes may start their own thread pools
  const auto max_threads = std::jthread::hardware_concurrency();

  for (uint8_t n = 1; n < max_threads; n++) {
    // notes:
    //  1. all threads run the same io_ctx
    //  2. objects are free to create their own strands, as needed
    //  3. use std::call_once to kickstart and sync thread start

    threads.emplace_back([&once = once, n = n, self = shared_from_this()] {
      std::call_once(once, [self] { self->kickstart(); }); // sync thread start

      self->nameThread(n); // give the thread a name
      self->io_ctx.run();  // run the io_ctx (returns when io_ctx canceled)
    });
  }

  Player::init(io_ctx);

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
      __LOG0("{} going out of scope reason={}\n", fnName(), ec.message());
      return;
    }

    __LOGX("{} executing\n", fnName());

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
