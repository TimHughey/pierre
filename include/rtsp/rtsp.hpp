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

#pragma once

#include <boost/asio/io_context.hpp>
#include <memory>
#include <thread>

#include "core/host.hpp"
#include "core/service.hpp"
#include "mdns/mdns.hpp"
#include "rtsp/aes_ctx.hpp"
#include "rtsp/server.hpp"
#include "rtsp/session.hpp"

namespace pierre {

// forward decl for shared_ptr typedef
class Rtsp;

typedef std::shared_ptr<Rtsp> sRtsp;

class Rtsp : public std::enable_shared_from_this<Rtsp> {
public:
  ~Rtsp();
  // shared_ptr API
  [[nodiscard]] static sRtsp create(sHost host) {
    // must call constructor directly since it's private
    return sRtsp(new Rtsp(host));
  }

  sRtsp getSelf() { return shared_from_this(); }

  // public API
  void join() { return _thread.join(); }
  void start();

private:
  Rtsp(sHost host);

  void runLoop();

private:
  // NOTE: order of member variables must match the order
  // created by the constructor

  // order dependent
  sHost host;
  sService service;
  smDNS mdns;

  // internal
  std::jthread _thread;

  // network
  boost::asio::io_context io_ctx;
  rtsp::sServer server;
};
} // namespace pierre