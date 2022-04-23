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
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/write.hpp>
#include <ctime>
#include <future>
#include <list>
#include <memory>
#include <string>
#include <thread>

#include "rtp/port_promise.hpp"

namespace pierre {
namespace rtp {

class Event; // forward decl for typedef
typedef std::shared_ptr<Event> sEvent;

class Event : public std::enable_shared_from_this<Event> {
public:
  using yield_context = boost::asio::yield_context;
  using io_service = boost::asio::io_service;
  using tcp_acceptor = boost::asio::ip::tcp::acceptor;
  using tcp_socket = boost::asio::ip::tcp::socket;

  typedef std::list<tcp_socket> SocketList;

public:
  ~Event();

public: // object creation and shared_ptr API
  [[nodiscard]] static sEvent create() {
    // not using std::make_shared; constructor is private
    return sEvent(new Event());
  }

  sEvent getPtr() { return shared_from_this(); }

public:
  // Public API

  void join() { return _thread.join(); }
  uint16_t localPort() const { return _port; }

  PortFuture start();

  std::thread &thread() { return _thread; }

private:
  Event();

  void doAccept(yield_context yield);
  void recvEvent(tcp_socket &socket, yield_context yield);

  void runLoop();

private:
  std::thread _thread{};
  std::thread::native_handle_type _handle;
  uint16_t _port = 0; // choose any port

  io_service _ioservice;
  tcp_acceptor *_acceptor;

  SocketList _sockets;

  PortPromise _port_promise;
};

} // namespace rtp
} // namespace pierre
