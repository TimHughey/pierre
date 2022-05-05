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
#include <string_view>

#include "rtp/anchor_info.hpp"
#include "rtp/rtp.hpp"
#include "rtp/servers.hpp"

namespace pierre {

using namespace std::chrono_literals;
using namespace std::string_view_literals;
using namespace rtp;

using io_context = boost::asio::io_context;
using error_code = boost::system::error_code;
using Clock = std::chrono::high_resolution_clock;
using Millis = std::chrono::duration<double, std::chrono::milliseconds::period>;

Rtp::Rtp()
    : servers({.io_ctx = io_ctx, .audio_raw = audio_raw}),
      pcm(PulseCodeMod::create({.audio_raw = audio_raw, .stream_info = _stream_info})) {
  // more later
}

uint16_t Rtp::localPort(ServerType type) {
  uint16_t port = 0;

  // note: requesting the local port starts the server
  switch (type) {
    case ServerType::Audio:
      port = servers.audio().localPort();
      break;

    case ServerType::Event:
      port = servers.event().localPort();
      break;

    case ServerType::Control:
      port = servers.control().localPort();
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
  _thread = std::jthread([this]() { runLoop(); });

  //  give this thread a name
  const auto handle = _thread.native_handle();
  pthread_setname_np(handle, "RTP");
}

void Rtp::save(const StreamData &data) {
  _stream_info = data;
  // _stream_info.dump();
}

Rtp::TeardownBarrier Rtp::teardown(Rtp::TeardownPhase phase) {
  if (false) { // debug
    auto fmt_str = FMT_STRING("{} started phase={}\n");
    std::string_view phase_sv;

    switch (phase) {
      case Rtp::TeardownPhase::None:
        phase_sv = "None";
        break;

      case Rtp::TeardownPhase::One:
        phase_sv = "ONE";
        break;

      case Rtp::TeardownPhase::Two:
        phase_sv = "TWO";
        break;
    }

    fmt::print(fmt_str, fnName(), phase_sv);
  }

  _teardown_phase = phase;

  // put a Teardown promise in the std::optional to trigger teardown
  return _teardown.emplace(Teardown()).get_future();
}

void Rtp::teardownFinished() {
  if (false) { // log before removing the barrier
    fmt::print(FMT_STRING("{}\n"), fnName());
  }

  _teardown.value().set_value(_teardown_phase); // remove the barrier
  _teardown.reset();                            // clear the promise
  _teardown_phase = None;
}

void Rtp::teardownNow() {
  // we are only here if the io_ctx is stopped
  // immediately tear everything down
  servers.teardown();

  // set all state objects to defqult
  _stream_info = StreamInfo();
  Anchor::use()->teardown();
  _input_info = InputInfo();
  ConnInfo::reset();

  teardownFinished();
}

void Rtp::watchForTeardown(WatchDog &watch_dog) {
  watch_dog.expires_after(250ms);

  watch_dog.async_wait([&]([[maybe_unused]] error_code ec) {
    // is there a promise waitng?
    if (_teardown.has_value()) {
      // what promise did we make? Phase 1 or 2?
      switch (_teardown_phase) {
        case Rtp::TeardownPhase::None:
          // odd but let's handle it
          teardownFinished();
          break;

        case Rtp::TeardownPhase::One:
          _stream_info.teardown(); // only reset the session key
          teardownFinished();
          break;

        case Rtp::TeardownPhase::Two:
          // this is the big phase, setting the vaalue and clearing promise
          // handled in teardownNow() after the io_ctx is stopped
          io_ctx.stop();
      }
    }

    watchForTeardown(watch_dog);
  });
}

std::shared_ptr<Rtp> pierre::Rtp::_instance;

} // namespace pierre
