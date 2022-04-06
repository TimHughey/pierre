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
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/write.hpp>
#include <ctime>
#include <list>
#include <memory>
#include <string>

namespace pierre {

class Rtsp {
public:
  using yield_context = boost::asio::yield_context;
  using io_service = boost::asio::io_service;
  using tcp_acceptor = boost::asio::ip::tcp::acceptor;
  using tcp_socket = boost::asio::ip::tcp::socket;

  typedef std::list<tcp_socket> SocketList;

public:
  Rtsp(uint16_t port) : _port(port) {}
  void run();

private:
  void doAccept(yield_context yield);
  void doRead(tcp_socket &socket, yield_context yield);
  void doWrite(tcp_socket &socket, yield_context yield);

  void session(tcp_socket &socket, auto request, yield_context yield);

private:
  uint16_t _port;

  io_service _ioservice;
  tcp_acceptor *_acceptor;

  SocketList _sockets;
  std::array<char, 4096> _data;
};
} // namespace pierre