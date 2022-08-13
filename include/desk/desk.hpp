/*
    Pierre - Custom Light Show via DMX for Wiss Landing
    Copyright (C) 2021  Tim Hughey

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

#include "base/elapsed.hpp"
#include "base/threads.hpp"
#include "base/typical.hpp"
#include "desk/dmx.hpp"
#include "desk/fx.hpp"
#include "frame/frame.hpp"
#include "io/io.hpp"
#include "mdns/mdns.hpp"

#include <exception>
#include <memory>
#include <optional>
#include <ranges>
#include <stop_token>
#include <vector>

namespace pierre {

class Desk;
typedef std::shared_ptr<Desk> shDesk;

namespace shared {
extern std::optional<shDesk> __desk;
constexpr auto &desk() { return __desk; }
} // namespace shared

class Desk : public std::enable_shared_from_this<Desk> {
private:
  static constexpr auto DESK_THREADS = 2; // +1 for main thread

private:
  Desk(); // must be defined in .cpp to hide FX includes

public: // static init, access to Desk
  static void init();
  static shDesk ptr() { return shared::desk().value()->shared_from_this(); }
  static void reset() { shared::desk().reset(); }
  static constexpr csv moduleID() { return module_id; }

public: // general API
  void handle_frame(shFrame frame);

  static void next_frame(shFrame frame) {
    auto self = ptr();

    asio::post(self->local_strand, [=] { self->handle_frame(frame); });
  }

  bool silence() const { return active_fx && active_fx->matchName(fx::SILENCE); }

private:
  bool connect() {
    if (zservice && !socket2.has_value()) {
      __LOG0(LCOL01 " attempting connect\n", moduleID(), "CONNECT");

      socket2.emplace(io_ctx);
      endpoint2.emplace(asio::ip::make_address_v4(zservice->address()), zservice->port());

      asio::async_connect(        // async connect
          *socket2,               // this socket
          std::array{*endpoint2}, // to this endpoint
          asio::bind_executor(    // serialize with
              local_strand, [self = ptr()](const error_code ec, const tcp_endpoint endpoint) {
                if (!ec) {

                  self->socket2->set_option(ip_tcp::no_delay(true));
                  self->endpoint_connected.emplace(endpoint);

                  self->log_connected();
                  self->watch_connection();

                } else {
                  self->reset_connection();
                }
              }));
    } else if (!zservice) {
      zservice = mDNS::zservice("_ruth._tcp");
    }

    return endpoint_connected.has_value() && socket2->is_open();
  }

  bool createSocket() {
    if (zservice && !socket.has_value()) {
      socket.emplace(io_ctx).open(ip_udp::v4());

      Port remote_port = zservice->txtVal<uint32_t>("msg_port");
      endpoint.emplace(asio::ip::make_address_v4(zservice->address()), remote_port);

      __LOG0(LCOL01 " created socket handle={}\n", moduleID(), "CREATE_SOCKET",
             socket->native_handle());
    } else if (!zservice) {
      zservice = mDNS::zservice(("_ruth._tcp"));
    }

    return socket.has_value() && socket->is_open();
  }

  void reset_connection() {
    asio::defer(local_strand, [self = ptr()] {
      error_code shut_ec;
      error_code close_ec;

      self->socket2->shutdown(tcp_socket::shutdown_both, shut_ec);
      self->socket2->close(close_ec);
      __LOG0(LCOL01 " socket error shutdown_reason={} close_reason={}\n", moduleID(), "WATCH",
             shut_ec.message(), close_ec.message());

      self->socket2.reset();
      self->endpoint2.reset();
      self->endpoint_connected.reset();
    });
  }

  void watch_connection() {
    socket2->async_wait(tcp_socket::wait_error, // wait for any error on the socket
                        [self = shared_from_this()]([[maybe_unused]] error_code ec) {
                          self->reset_connection();

                          self->connect();
                        });
  }

  // misc debug
  void log_connected() {
    __LOG0(LCOL01 " connected handle={} {}:{} -> {}:{}\n", moduleID(), "CONNECT",
           socket2->native_handle(), socket2->local_endpoint().address().to_string(),
           socket2->local_endpoint().port(), endpoint_connected->address().to_string(),
           endpoint_connected->port());
  }

private:
  // order dependent
  io_context io_ctx;
  strand local_strand;
  shFX active_fx;
  shZeroConfService zservice;
  std::shared_ptr<work_guard> guard;

  // order independent
  Thread thread_main;
  Threads threads;
  std::stop_token stop_token;

  std::optional<udp_socket> socket;
  std::optional<udp_endpoint> endpoint;
  std::optional<tcp_socket> socket2;
  std::optional<tcp_endpoint> endpoint2;
  std::optional<tcp_endpoint> endpoint_connected;

  static constexpr csv module_id = "DESK";
};

} // namespace pierre
