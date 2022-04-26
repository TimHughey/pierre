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

#include <exception>
#include <fmt/format.h>

#include "rtp/anchor_info.hpp"
#include "rtp/audio/buffered/server.hpp"
#include "rtp/control/datagram.hpp"
#include "rtp/event/server.hpp"
#include "rtp/rtp.hpp"

namespace pierre {
using namespace rtp;

Rtp::Rtp()
    : servers{.event = rtp::event::Server::create(io_ctx),
              .audio_buffered = rtp::audio::buffered::Server::create(io_ctx),
              .control = rtp::control::Datagram::create(io_ctx),
              .timing = rtp::timing::Datagram::create(io_ctx)} {
  // more later
}

void Rtp::runLoop() {
  using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

  WorkGuard work_guard(io_ctx.get_executor());
  io_ctx.run(); // returns until no more work

  auto self = pthread_self();

  std::array<char, 24> thread_name{0};
  pthread_getname_np(self, thread_name.data(), thread_name.size());
  fmt::print("{} work is complete\n", thread_name.data());
}

uint16_t Rtp::localPort(ServerType type) {
  uint16_t port = 0;

  switch (type) {
    case AudioBuffered:
      port = servers.audio_buffered->localPort();
      break;

    case Event:
      port = servers.event->localPort();
      break;

    case Control:
      port = servers.control->localPort();
      break;

    case Timing:
      port = servers.timing->localPort();
      break;
  }

  return port;
}

void Rtp::start() {
  _thread = std::jthread([this]() {
    auto self = pthread_self();
    pthread_setname_np(self, "RTP");

    runLoop();
  });
}

void Rtp::saveAnchorInfo(const rtp::AnchorData &data) {
  _anchor = data;
  _anchor.dump();
}

void Rtp::saveSessionInfo(csr shk, csr active_remote, csr dacp_id) {
  _session_key = shk;
  _active_remote = active_remote;
  _dacp_id = dacp_id;
}

std::shared_ptr<Rtp> pierre::Rtp::_instance;

} // namespace pierre