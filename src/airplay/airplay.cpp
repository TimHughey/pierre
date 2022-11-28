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
#include "frame/master_clock.hpp"
#include "rtsp.hpp"
#include "server/servers.hpp"

#include <algorithm>
#include <latch>

namespace pierre {

static std::shared_ptr<Airplay> overlord;
static std::shared_ptr<Rtsp> rtsp;

std::shared_ptr<Airplay> Airplay::init() noexcept { // static
  auto s = std::shared_ptr<Airplay>(new Airplay());

  // executed by caller thread
  airplay::Servers::init(s->io_ctx);

  std::latch latch(AIRPLAY_THREADS);

  for (auto n = 0; n < AIRPLAY_THREADS; n++) {
    s->threads.emplace_back([=, &latch, s = s->shared_from_this()](std::stop_token token) mutable {
      s->tokens.add(std::move(token));

      name_thread("Airplay", n);

      // we want all airplay threads to start at once
      latch.count_down();
      s->io_ctx.run();
    });
  }

  latch.wait();   // wait for all threads to start
  s->watch_dog(); // start the watchdog once all threads are started

  shared::master_clock->peers_reset(); // reset timing peers

  // start listening for Rtsp messages
  rtsp = Rtsp::init(s->io_ctx);

  overlord = std::move(s);

  INFO(module_id, "INIT", "sizeof={} threads={}/{} features={:#x}\n", sizeof(Airplay), 0,
       AIRPLAY_THREADS, Features().ap2Default());

  return overlord->shared_from_this();
}

std::shared_ptr<Airplay> &Airplay::self() noexcept { return overlord; }

void Airplay::watch_dog() noexcept {
  // cancels any running timers
  [[maybe_unused]] auto timers_cancelled = watchdog_timer.expires_after(2s);

  watchdog_timer.async_wait([s = ptr()](const error_code ec) {
    if (ec == errc::success) { // unless success, fall out of scape
      s->tokens.any_requested(s->io_ctx, s->guard, [=]() { s->watch_dog(); });

    } else {
      INFO(module_id, "WATCH DOG", "going out of scope reason={}\n", ec.message());
    }
  });
}

} // namespace pierre
