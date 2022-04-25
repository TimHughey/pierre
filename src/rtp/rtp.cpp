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
  io_ctx.run(); // returns until no more work

  std::array<char, 24> thread_name{0};
  pthread_getname_np(_thread.native_handle(), thread_name.data(), thread_name.size());
  fmt::print("{} has run out of work\n", thread_name.data());
}

rtp::PortFuture Rtp::startServer(ServerType type) {
  switch (type) {
    case AudioBuffered:
      return servers.audio_buffered->start();

    case Event:
      return servers.event->start();

    case Control:
      return servers.control->start();

    case Timing:
      return servers.timing->start();
  }

  throw(std::runtime_error("unhandled enum"));
}

void Rtp::start() {
  _thread = std::thread([this]() { runLoop(); });
  pthread_setname_np(_thread.native_handle(), "RTP");
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