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

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <cstdint>
// #include <list>
#include <memory>
#include <optional>

#include "core/host.hpp"
#include "core/service.hpp"
#include "mdns/mdns.hpp"
#include "nptp/nptp.hpp"
#include "rtsp/session.hpp"

namespace pierre {
namespace rtsp {

// forward decl for shared_ptr
class Server;
typedef std::shared_ptr<Server> sServer;

class Server : public std::enable_shared_from_this<Server> {
public:
  using io_context = boost::asio::io_context;
  using tcp_acceptor = boost::asio::ip::tcp::acceptor;
  using tcp_endpoint = boost::asio::ip::tcp::endpoint;
  using tcp_socket = boost::asio::ip::tcp::socket;
  using ip_tcp = boost::asio::ip::tcp;

public:
  struct Opts {
    io_context &io_ctx;
    sHost host = nullptr;
    sService service = nullptr;
    smDNS mdns = nullptr;
    sNptp nptp = nullptr;
  };

public:
  // shared_ptr API
  [[nodiscard]] static sServer create(const Opts &opts) {
    // must call constructor directly since it's private
    return sServer(new Server(opts));
  }

  sServer getSelf() { return shared_from_this(); }

  // public API
  void start();

private:
  Server(const Opts &opts);

  void async_accept();

private:
  const Opts &opts;
  io_context &io_ctx;
  tcp_acceptor acceptor;
  // std::list<sSession> _sessions;  // session storage
  // std::list<tcp_socket> _sockets; // socket storage

  std::optional<tcp_socket> socket;

  static constexpr uint16_t _port = 7000;
};

} // namespace rtsp
} // namespace pierre