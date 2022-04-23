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

#include <array>
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/write.hpp>
#include <ctime>
#include <list>
#include <memory>
#include <string>
#include <thread>

#include "core/host.hpp"
#include "core/service.hpp"
#include "mdns/mdns.hpp"
#include "rtp/rtp.hpp"
#include "rtsp/aes_ctx.hpp"
#include "rtsp/nptp.hpp"
#include "rtsp/server.hpp"
#include "rtsp/session.hpp"

namespace pierre {

// forward decl for shared_ptr typedef
class Rtsp;

typedef std::shared_ptr<Rtsp> sRtsp;
typedef std::shared_ptr<std::thread> stRtsp;

class Rtsp : public std::enable_shared_from_this<Rtsp> {
public:
  using yield_context = boost::asio::yield_context;
  using io_service = boost::asio::io_service;
  using io_context = boost::asio::io_context;
  using tcp_acceptor = boost::asio::ip::tcp::acceptor;
  using tcp_socket = boost::asio::ip::tcp::socket;

  typedef std::list<tcp_socket> SocketList;

public:
  ~Rtsp();

  // shared_ptr API
  [[nodiscard]] static sRtsp create(sHost host) {
    // must call constructor directly since it's private
    return sRtsp(new Rtsp(host));
  }

  sRtsp getPtr() { return shared_from_this(); }

  // public API
  void join() { return _thread.join(); }
  void start();

private:
  Rtsp(sHost host);

  // void doAccept(yield_context yield);
  // void doRead(tcp_socket &socket, yield_context yield);
  // void doWrite(tcp_socket &socket, yield_context yield);

  void runLoop();
  // void session(tcp_socket &socket, auto request);

private:
  // NOTE: order of member variables must match the order
  // created by the constructor

  // required config, data and network services
  sHost host;
  sService service;
  smDNS mdns;
  rtsp::sNptp nptp;
  sRtp rtp;

  // internal

  std::thread _thread;
  uint16_t _port;

  std::thread::native_handle_type _handle;

  // io_service _ioservice;
  tcp_acceptor *_acceptor;

  SocketList _sockets;

  io_context io_ctx;
  rtsp::sServer server;
};
} // namespace pierre