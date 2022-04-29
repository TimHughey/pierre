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

#include <chrono>
#include <exception>
#include <fmt/format.h>

#include "rtp/anchor_info.hpp"
#include "rtp/audio/buffered/server.hpp"
#include "rtp/control/datagram.hpp"
#include "rtp/event/server.hpp"
#include "rtp/rtp.hpp"
#include "rtp/servers.hpp"

namespace pierre {

using namespace std::chrono_literals;
using io_context = boost::asio::io_context;
using error_code = boost::system::error_code;
using namespace rtp;

Rtp::Rtp() : depot(ServerDepot(io_ctx)) {
  // more later
}

void Rtp::createServers() {
  servers.event = event::Server::create(io_ctx);
  servers.audio_buffered = audio::buffered::Server::create({.io_ctx = io_ctx, .anchor = _anchor});
  servers.control = control::Datagram::create(io_ctx);
  servers.timing = timing::Datagram::create(io_ctx);
}

uint16_t Rtp::localPort(ServerType type) {
  uint16_t port = 0;

  switch (type) {
    case AudioBuffered:
      port = servers.audio_buffered->localPort();
      break;

    case ServerType::Event:
      port = servers.event->localPort();
      break;

    case ServerType::Control:
      port = servers.control->localPort();
      break;

    case ServerType::Timing:
      port = servers.timing->localPort();
      break;
  }

  return port;
}

void Rtp::runLoop() {
  running = true;

  do {
    WatchDog watch_dog(io_ctx);
    watchForTeardown(watch_dog);

    io_ctx.run();

    if (_teardown.has_value()) {
      teardownNow();
    }

    io_ctx.reset();

  } while (running);

  auto self = pthread_self();

  std::array<char, 24> thread_name{0};
  pthread_getname_np(self, thread_name.data(), thread_name.size());
  fmt::print("{} work is complete\n", thread_name.data());
}

void Rtp::start() {
  createServers();

  _thread = std::jthread([this]() { runLoop(); });

  //  give this thread a name
  const auto handle = _thread.native_handle();
  pthread_setname_np(handle, "RTP");
}

void Rtp::save(const rtp::AnchorData &data) {
  _anchor = data;
  // _anchor.dump();
}

void Rtp::save(const rtp::StreamData &data) {
  _stream_info = data;
  // _stream_info.dump();
}

Rtp::TeardownBarrier Rtp::teardown() {
  fmt::print(FMT_STRING("{} teardown initiated\n"), fnName());

  return _teardown.emplace(Teardown()).get_future();
}

void Rtp::teardownNow() {
  constexpr auto attempts = 10;

  // give io_context an opportunity to shutdown gracefully
  for (auto attempt = 0; (io_ctx.stopped() == false) && (attempt < attempts); attempt++) {
    std::this_thread::sleep_for(10ms);
  }

  fmt::print(FMT_STRING("{} io_ctx stopped\n"), fnName());

  // reset all shared pointers
  servers.event.reset();
  servers.audio_buffered.reset();
  servers.control.reset();
  servers.timing.reset();

  // create new io context and servers
  createServers();

  // remove the barrier
  _teardown.value().set_value(true);

  // clear the promise
  _teardown.reset();

  fmt::print(FMT_STRING("{} teardown complete\n"), fnName());
}

void Rtp::watchForTeardown(WatchDog &watch_dog) {
  watch_dog.expires_after(250ms);

  watch_dog.async_wait([&]([[maybe_unused]] error_code ec) {
    if (_teardown.has_value()) {
      io_ctx.stop();
      return;
    }

    watchForTeardown(watch_dog);
  });
}

std::shared_ptr<Rtp> pierre::Rtp::_instance;

} // namespace pierre
