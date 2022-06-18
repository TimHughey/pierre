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
#include "dmx/render.hpp"
#include "frames_request.hpp"
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

void nameThread(auto num) {
  const auto handle = pthread_self();
  const auto name = fmt::format("Player {}", num);

  pthread_setname_np(handle, name.c_str());
}

namespace shared {
std::optional<shPlayer> __player;
std::optional<shPlayer> &player() { return __player; }
} // namespace shared

using error_code = boost::system::error_code;
namespace asio = boost::asio;
namespace errc = boost::system::errc;
namespace ranges = std::ranges;

Player::Player(asio::io_context &io_ctx)
    : local_strand(io_ctx),                               // player serialized work
      decode_strand(io_ctx),                              // decoder serialized work
      spooler_strand(io_ctx),                             // spooler serialized work
      watchdog_timer(dsp_io_ctx),                         // dsp threads shutdown
      spooler(player::Spooler::create(io_ctx, moduleId)), // spooler
      frames_request(player::FramesRequest::create()) {
  frames_request->src = spooler;
  // can not call member functions that use shared_from_this() in constructor
}

// static methods for creating, getting and resetting the shared instance
shPlayer Player::init(asio::io_context &io_ctx) {
  auto self = shared::player().emplace(new Player(io_ctx));

  dmx::Render::init(io_ctx, self->frames_request);
  self->start();

  return self->shared_from_this();
}

shPlayer Player::ptr() { return shared::player().value()->shared_from_this(); }
void Player::reset() { shared::player().reset(); }

// general API and member functions
void Player::accept(packet::Basic &packet) { // static
  auto self = ptr();
  shRTP frame = RTP::create(packet);

  frame->dump(false);

  if (frame->keep(self->_flush)) {
    // 1. NOT flushed
    // 2. decipher OK

    self->spooler->queueFrame(frame);

    // async decode
    asio::post(self->decode_strand, // serialize decodes due to single av ctx
               [frame = frame, &dsp_io_ctx = self->dsp_io_ctx]() mutable {
                 RTP::decode(frame); // OK to run on separate thread, shared_ptr

                 // find peaks using any available thread
                 asio::post(dsp_io_ctx, [frame = frame]() mutable {
                   RTP::findPeaks(frame);
                   frame->cleanup();
                 });
               });
  }
}
void Player::flush(const FlushRequest &request) { // static
  ptr()->spooler->flush(request);
  dmx::Render::ptr()->flush(request);
}

void Player::run() {
  nameThread(0);
  watchDog();

  // to maximizing system utilization hardware_concurrency() * 2 threads
  // are started.  however, we know other classes will also start a thread
  // pool of their own so we only use hardware_concurrency
  const auto max_threads = std::jthread::hardware_concurrency();

  for (uint8_t n = 1; n < max_threads; n++) {
    // notes:
    //  1. all threads run the same io_ctx
    //  2. objects are free to create their own strands, as needed

    dsp_threads.emplace_back([n = n, self = shared_from_this()] {
      nameThread(n);          // give the thread a name
      self->dsp_io_ctx.run(); // dsp processing thread
    });
  }

  dsp_io_ctx.run(); // include the Player thread in the dsp pool
  // returns when io_ctx does not have scheduled work
}

void Player::saveAnchor(anchor::Data &data) { // static
  Anchor::ptr()->save(data);

  ptr()->_is_playing = data.playing();
  dmx::Render::playMode(data.playing());
}

void Player::shutdown() { // static
  auto self = ptr();

  self->dsp_thread.request_stop();

  ranges::for_each(self->dsp_threads, [](Thread &thread) mutable {
    // wait for each thread to complete
    thread.join();
  });

  self->dsp_thread.join(); // wait for dsp thread to complete
}

void Player::start() { // simply start the dsp main thread and return
  dsp_thread = Thread(&Player::run, this);
}

void Player::teardown() { // static
  ptr()->packet_count = 0;
  dmx::Render::teardown();
}

void Player::watchDog() {
  // cancels any running timers
  [[maybe_unused]] auto timers_cancelled = watchdog_timer.expires_after(250ms);

  watchdog_timer.async_wait([self = shared_from_this()](const error_code ec) {
    if (ec != errc::success) { // any error, fall out of scope
      __LOG0("{} going out of scope reason={}\n", fnName(), ec.message());
      return;
    }

    __LOGX("{} executing\n", fnName());

    auto stop_token = self->dsp_thread.get_stop_token();

    if (stop_token.stop_requested()) { // thread wants to stop
      self->dsp_io_ctx.stop();         // stop the io_ctx (stops other threads)
      return;                          // fall out of scope
    }

    self->watchDog(); // restart the timer (keeps us in scope)
  });
}

} // namespace pierre
