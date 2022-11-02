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
#include "base/logger.hpp"
#include "common/ss_inject.hpp"
#include "conn_info/conn_info.hpp"
#include "frame/master_clock.hpp"
#include "server/servers.hpp"

#include <algorithm>
#include <latch>
#include <ranges>

namespace pierre {

namespace {
namespace ranges = std::ranges;
}

namespace shared {
std::shared_ptr<Airplay> airplay;
} // namespace shared

shAirplay Airplay::init_self() {
  INFO(module_id, "INIT", "features={:#x}\n", Features().ap2Default());

  // executed by caller thread
  airplay::ConnInfo::init();
  airplay::Servers::init(io_ctx);

  std::latch latch(AIRPLAY_THREADS);

  for (auto n = 0; n < AIRPLAY_THREADS; n++) {
    threads.emplace_back([=, &latch, self = shared_from_this()](std::stop_token token) mutable {
      self->tokens.add(std::move(token));

      name_thread("Airplay", n);

      // we want all airplay threads to start at once
      latch.arrive_and_wait();
      self->io_ctx.run();
    });
  }

  latch.wait(); // wait for all threads to start
  watch_dog();  // start the watchdog once all threads are started

  shared::master_clock->peers_reset(); // reset timing peers

  // start listening for Rtsp messages
  airplay::Servers::ptr()->localPort(ServerType::Rtsp);

  return shared_from_this();
}

void Airplay::watch_dog() {
  // cancels any running timers
  [[maybe_unused]] auto timers_cancelled = watchdog_timer.expires_after(250ms);

  watchdog_timer.async_wait([self = shared_from_this()](const error_code ec) {
    if (ec == errc::success) { // unless success, fall out of scape
      self->tokens.any_requested(self->io_ctx, self->guard, [=]() { self->watch_dog(); });

    } else {
      INFO(module_id, "WATCH DOG", "going out of scope reason={}\n", ec.message());
    }
  });
}

} // namespace pierre
