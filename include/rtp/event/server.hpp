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
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <future>
#include <memory>
#include <optional>
#include <string>

#include "rtp/port_promise.hpp"

namespace pierre {
namespace rtp {
namespace event {

class Server; // forward decl for typedef
typedef std::shared_ptr<Server> sServer;

class Server : public std::enable_shared_from_this<Server> {
public:
  using io_context = boost::asio::io_context;
  using tcp_acceptor = boost::asio::ip::tcp::acceptor;
  using tcp_endpoint = boost::asio::ip::tcp::endpoint;
  using tcp_socket = boost::asio::ip::tcp::socket;
  using ip_tcp = boost::asio::ip::tcp;

  typedef const char *ccs;

public:
  ~Server();

public: // object creation and shared_ptr API
  [[nodiscard]] static sServer create(io_context &io_ctx) {
    // not using std::make_shared; constructor is private
    return sServer(new Server(io_ctx));
  }

  sServer getSelf() { return shared_from_this(); }

public:
  // Public API
  uint16_t localPort() const { return _port; }
  PortFuture start();

private:
  Server(io_context &io_ctx);

  void asyncAccept();

private:
  static ccs fnName(std::source_location loc = std::source_location::current()) {
    return loc.function_name();
  }

private:
  // order dependent
  io_context &io_ctx;
  tcp_acceptor acceptor;

  // temporary holder of socket (io_ctx) which waiting for
  // a connection
  std::optional<tcp_socket> socket;

  uint16_t _port = 0; // choose any port

  PortPromise _port_promise;
};

} // namespace event
} // namespace rtp
} // namespace pierre
