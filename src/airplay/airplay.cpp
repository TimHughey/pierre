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

#include "airplay/airplay.hpp"
#include "base/features.hpp"
#include "base/host.hpp"
#include "base/input_info.hpp"
#include "base/logger.hpp"
#include "common/ss_inject.hpp"
#include "config/config.hpp"
#include "conn_info/conn_info.hpp"
#include "frame/anchor.hpp"
#include "frame/master_clock.hpp"
#include "server/servers.hpp"

#include <latch>

namespace pierre {

namespace shared {
std::optional<shAirplay> __airplay;
} // namespace shared

shAirplay Airplay::init() { // static
  auto self = shared::__airplay.emplace(new Airplay());
  INFO(module_id, "INIT", "features={:#x}\n", Features().ap2Default());

  // executed by caller thread
  airplay::ConnInfo::init();
  airplay::Servers::init({.io_ctx = self->io_ctx});

  std::latch threads_latch(AIRPLAY_THREADS);
  self->thread_main = Thread([=, &threads_latch](std::stop_token token) {
    name_thread("Airplay", 0); // main thread
    self->stop_token = token;

    // add some work to io_ctx so threads don't exit immediately
    self->watch_dog();

    // start remaining threads
    for (auto n = 1; n < AIRPLAY_THREADS; n++) {
      self->threads.emplace_back([=, &threads_latch] {
        name_thread("Airplay", n);
        threads_latch.count_down();
        self->io_ctx.run();
      });
    }

    threads_latch.count_down();
    self->io_ctx.run(); // run io_ctx on main airplay thread
  });

  threads_latch.wait();                // wait for all threads to start
  shared::master_clock->peers_reset(); // reset timing peers

  // start listening for Rtsp messages
  airplay::Servers::ptr()->localPort(ServerType::Rtsp);

  return self->shared_from_this();
}

void Airplay::watch_dog() {
  // cancels any running timers
  [[maybe_unused]] auto timers_cancelled = watchdog_timer.expires_after(250ms);

  watchdog_timer.async_wait([&, self = shared_from_this()](const error_code ec) {
    if (ec == errc::success) { // unless success, fall out of scape

      // check if any thread has received a stop request
      if (self->stop_token.stop_requested()) {
        self->io_ctx.stop();
      } else {
        self->watch_dog();
      }
    } else {
      INFO(module_id, "WATCH DOG", "going out of scope reason={}\n", ec.message());
    }
  });
}

} // namespace pierre
