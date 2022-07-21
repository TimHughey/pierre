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
#include <optional>
#include <ranges>

using namespace std::chrono_literals;
using namespace pierre::player;

namespace pierre {

static void name_thread(csv name, auto num) {
  const auto handle = pthread_self();
  const auto fullname = fmt::format("{} {}", name, num);

  __LOG0(LCOL01 " handle={} name={}\n", Player::moduleID(), "NAME_THREAD", handle, fullname);

  pthread_setname_np(handle, fullname.c_str());
}

namespace shared {
std::optional<shPlayer> __player;
std::optional<shPlayer> &player() { return __player; }
} // namespace shared

namespace ranges = std::ranges;

// static methods for creating, getting and resetting the shared instance
shPlayer Player::init(io_context &io_ctx) { // static
  auto self = shared::player().emplace(new Player(io_ctx));

  player::Render::init(io_ctx, self->spooler);
  mDNS::browse(Config::object("player")["dmx_service"]); // start async browse
  return self->start();
}
shPlayer Player::ptr() { return shared::player().value()->shared_from_this(); }
void Player::reset() { shared::player().reset(); }

// general API and member functions
void Player::accept(uint8v &packet) { // static
  auto self = ptr();
  shFrame frame = Frame::create(packet);

  __LOGX("{}\n", Frame::inspect(frame));

  if (frame->keep(self->flush_request)) {
    // 1. NOT flushed
    // 2. decipher OK

    self->spooler->queueFrame(frame);

    // async decode
    asio::post(self->decode_strand, // serialize decodes due to single av ctx
               [frame = frame, &dsp_io_ctx = self->io_ctx_dsp]() mutable {
                 Frame::decode(frame); // OK to run on separate thread, shared_ptr

                 // find peaks using any available thread
                 asio::post(dsp_io_ctx, [frame = frame]() mutable {
                   Frame::findPeaks(frame);
                   frame->cleanup();
                 });
               });
  }
}

shPlayer Player::start() {
  static std::once_flag once;

  // to maximizing system utilization hardware_concurrency() threads
  // are started.  however, we know other classes will also start a thread
  // pool of their own so we only use hardware_concurrency
  float factor = Config::object("player")["dsp"]["concurrency_factor"];

  __LOG(LCOL01 " factor={}\n", Player::moduleID(), "START", factor);
  for (uint8_t n = 0; n < std::jthread::hardware_concurrency() * factor; n++) {
    // notes:
    //  1. start the dsp threads
    //  2. all threads run the same io_ctx
    //  3. objects are free to create their own strands, as needed

    std::call_once(once, [this]() { watchDog(); });

    dsp_threads.emplace_back([n = n, self = shared_from_this()] {
      name_thread("DSP", n);  // give the thread a name
      self->io_ctx_dsp.run(); // dsp processing thread
    });
  }

  return shared_from_this();
}

void Player::saveAnchor(anchor::Data &data) { // static, must be in .cpp
  Anchor::ptr()->save(data);

  auto self = ptr();
  self->play_mode = data.playing() ? PLAYING : NOT_PLAYING;
  player::Render::playMode(self->play_mode);
}

void Player::shutdown() { // static
  auto &threads = ptr()->dsp_threads;
  auto &stop_thread = threads.front(); // signal the first thread

  stop_thread.request_stop();                             // stop caught by watchDog()
  ranges::for_each(threads, [](Thread &t) { t.join(); }); // wait for threads to stop
}

void Player::teardown() { // static, must be in .cpp
  ptr()->packet_count = 0;
  player::Render::teardown();
}

void Player::watchDog() {
  // cancels any running timers
  [[maybe_unused]] auto timers_cancelled = watchdog_timer.expires_after(250ms);

  watchdog_timer.async_wait([self = shared_from_this()](const error_code ec) {
    if (ec == errc::success) { // unless success, fall out of scape

      auto &stop_thread = self->dsp_threads.front();       // the first thread is signaled to stop
      if (stop_thread.get_stop_token().stop_requested()) { // thread wants to stop
        self->io_ctx_dsp.stop();                           // stops io_ctx and threads
      } else {
        self->watchDog(); // restart the timer (keeps us in scope)
      }
    } else {
      __LOG0(LCOL01 " going out of scope reason={}\n", moduleID(), csv("WATCH_DOG"), ec.message());
    }
  });
}

} // namespace pierre
