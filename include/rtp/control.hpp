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

#pragma once

#include <array>
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/write.hpp>
#include <ctime>
#include <list>
#include <memory>
#include <string>
#include <thread>

#include "rtp/port_promise.hpp"

namespace pierre {
namespace rtp {

class Control; // forward decl for typedef
typedef std::shared_ptr<Control> sControl;

class Control : public std::enable_shared_from_this<Control> {
public:
  using yield_context = boost::asio::yield_context;
  using io_service = boost::asio::io_service;
  using udp_endpoint = boost::asio::ip::udp::endpoint;
  using udp_socket = boost::asio::ip::udp::socket;

public:
  ~Control();

public: // object creation and shared_ptr API
  [[nodiscard]] static sControl create() {
    // not using std::make_shared; constructor is private
    return sControl(new Control());
  }

  sControl getPtr() { return shared_from_this(); }

public:
  // Public API

  void join() { return _thread.join(); }
  uint16_t localPort() const;

  PortFuture start();

  std::thread &thread() { return _thread; }

private:
  Control();

  void recvPacket(yield_context yield);

  void runLoop();

private:
  std::thread _thread{};
  std::thread::native_handle_type _handle;
  const uint16_t _port = 0; // choose any port

  io_service _ioservice;
  udp_socket _socket;
  udp_endpoint _remote_endpoint;

  PortPromise _port_promise;
};

} // namespace rtp
} // namespace pierre
