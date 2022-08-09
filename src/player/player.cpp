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

#include "player.hpp"
#include "base/elapsed.hpp"
#include "base/threads.hpp"
#include "config/config.hpp"
#include "mdns/mdns.hpp"
#include "render.hpp"
#include "rtp_time/anchor.hpp"
#include "spooler.hpp"

#include <algorithm>
#include <boost/asio.hpp>
#include <chrono>
#include <fmt/format.h>
#include <iterator>
#include <latch>
#include <optional>
#include <ranges>
#include <stop_token>

using namespace std::chrono_literals;
using namespace pierre::player;

namespace pierre {

namespace shared {
std::optional<shPlayer> __player;
std::optional<shPlayer> &player() { return __player; }
} // namespace shared

// static methods for creating, getting and resetting the shared instance
void Player::init() { // static
  auto self = shared::__player.emplace(new Player());

  player::Render::init(self->io_ctx, self->spooler);     // adds work to the player io_ctx
  mDNS::browse(Config::object("player")["dmx_service"]); // start async browse

  float dsp_hw_factor = Config::object("player")["dsp"]["concurrency_factor"];
  int dsp_threads = std::jthread::hardware_concurrency() * dsp_hw_factor;
  int thread_count = dsp_threads + PLAYER_THREADS;
  std::latch threads_latch{thread_count};

  // start the main player thread
  self->thread_main = Thread([=, &threads_latch](std::stop_token token) {
    self->stop_token = token;
    name_thread("PLAYER MAIN");

    self->watch_dog(); // schedule work for player dsp context

    for (auto n = 0; n < PLAYER_THREADS + dsp_threads; n++) {
      // notes:
      //  1. start the dsp threads (unique io_ctx)
      //  2. starts player threads (unique io_ctx)
      //  3. objects are free to create their own strands, as needed

      if (n < dsp_threads) {
        self->threads.emplace_back([=, &threads_latch, self = ptr()] {
          name_thread("Player DSP", n);
          threads_latch.count_down();
          self->io_ctx_dsp.run(); // dsp io_ctx
        });
      } else {
        self->threads.emplace_back([=, &threads_latch, self = ptr()] {
          name_thread("Player", n - dsp_threads); // give the thread a name
          threads_latch.count_down();
          self->io_ctx.run(); // player io_ctx
        });
      }
    }

    self->io_ctx.run(); // final player thread
  });

  threads_latch.wait();
}

shPlayer Player::ptr() { return shared::player().value()->shared_from_this(); }
void Player::reset() { shared::player().reset(); }

// general API and member functions
void Player::accept(uint8v &packet) { // static
  auto self = ptr();
  shFrame frame = Frame::create(packet);

  __LOGX("{}\n", Frame::inspect(frame));

  // keep() returns true when:  a) not flushed;  b) deciphered OK
  if (frame->keep(self->flush_request)) {
    asio::post(           // async dsp
        self->io_ctx_dsp, // use a thread pool, finding peaks takes awhile
        [frame = self->spooler->queueFrame(frame)]() { Frame::findPeaks(frame); });
  }
}

void Player::saveAnchor(anchor::Data &data) { // static, must be in .cpp
  Anchor::ptr()->save(data);

  auto self = ptr();
  self->play_mode = data.playing() ? PLAYING : NOT_PLAYING;
  player::Render::playMode(self->play_mode);
}

void Player::teardown() { // static, must be in .cpp
  ptr()->packet_count = 0;
  // player::Render::teardown();
}

void Player::watch_dog() {
  // cancels any running timers
  [[maybe_unused]] auto timers_cancelled = watchdog_timer.expires_after(250ms);

  watchdog_timer.async_wait([&, self = shared_from_this()](const error_code ec) {
    if (ec == errc::success) { // unless success, fall out of scape

      // check if any thread has received a stop request
      if (self->stop_token.stop_requested()) {
        self->stop_io();
      } else {
        self->watch_dog();
      }
    } else {
      __LOG0(LCOL01 " going out of scope reason={}\n", moduleID(), csv("WATCH_DOG"), ec.message());
    }
  });
}

} // namespace pierre
