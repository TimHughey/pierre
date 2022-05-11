/*
    Pierre - Custom Light Show for Wiss Landing
    Copyright (C) 2022  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#include <array>
#include <fmt/format.h>
#include <pthread.h>

#include "core/service.hpp"
#include "rtp/rtp.hpp"
#include "rtsp/aes_ctx.hpp"
#include "rtsp/reply.hpp"
#include "rtsp/rtsp.hpp"

namespace pierre {
using namespace rtsp;

Rtsp::Rtsp(sHost _host)
    : host(_host),                    // local copy of Host shared ptr
      service(Service::create(host)), // create Service
      mdns(mDNS::create(service))     // create mDNS
{}

Rtsp::~Rtsp() {
  fmt::print("Rtsp destructor called\n");
  // need cleanup code
}

void Rtsp::start() {
  fmt::print("\nPierre {:>25}\n\n", host->firmwareVerson());

  mdns->start();

  _thread = std::jthread([this]() { runLoop(); });
  pthread_setname_np(_thread.native_handle(), "RTSP");
}

void Rtsp::runLoop() {
  // create and start Rtp, it will be needed later
  auto rtp = Rtp::create();
  rtp->start();

  // create server
  server =
      rtsp::Server::create({.io_ctx = io_ctx, .host = host, .service = service, .mdns = mdns});
  server->start();

  io_ctx.run(); // returns until no more work

  std::array<char, 24> thread_name{0};
  pthread_getname_np(_thread.native_handle(), thread_name.data(), thread_name.size());
  fmt::print("{} has run out of work\n", thread_name.data());
}
} // namespace pierre