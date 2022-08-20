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
#include "base/uint8v.hpp"
#include "desk/dmx.hpp"
#include "desk/fx.hpp"
#include "frame/frame.hpp"
#include "io/io.hpp"
#include "mdns/mdns.hpp"

#include <array>
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
  static constexpr auto DESK_THREADS = 3; // +1 for main thread

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

    // recalc the sync wait to account for lag
    auto sync_wait = frame->calc_sync_wait(self->latency);

    if (sync_wait < Nanos::zero()) {
      sync_wait = Nanos::zero();
    }

    // schedule the release
    self->release_timer.expires_after(sync_wait);
    self->release_timer.async_wait(
        [self = std::move(self), frame = std::move(frame)](const error_code &ec) {
          if (!ec) {
            self->handle_frame(frame);
          }
        });
  }

  bool silence() const { return active_fx && active_fx->matchName(fx::SILENCE); }

private:
  bool connect() {
    if (zservice && !ctrl_socket.has_value()) {
      __LOG0(LCOL01 " attempting connect\n", moduleID(), "CONNECT");

      ctrl_socket.emplace(io_ctx);
      ctrl_endpoint.emplace(asio::ip::make_address_v4(zservice->address()), zservice->port());

      asio::async_connect(            // async connect
          *ctrl_socket,               // this socket
          std::array{*ctrl_endpoint}, // to this endpoint
          asio::bind_executor(        // serialize with
              local_strand, [self = ptr()](const error_code ec, const tcp_endpoint endpoint) {
                if (!ec) {
                  self->ctrl_socket->set_option(ip_tcp::no_delay(true));
                  self->endpoint_connected.emplace(endpoint);

                  self->log_connected();
                  self->watch_connection();
                  self->setup_connection();

                } else {
                  self->reset_connection();
                }
              }));
    } else if (!zservice) {
      zservice = mDNS::zservice("_ruth._tcp");
    }

    return endpoint_connected.has_value() && ctrl_socket->is_open();
  }

  void reset_connection();
  void setup_connection();
  void watch_connection();

  // misc debug
  void log_connected() {
    __LOG0(LCOL01 " connected handle={} {}:{} -> {}:{}\n", moduleID(), "CONNECT",
           ctrl_socket->native_handle(), ctrl_socket->local_endpoint().address().to_string(),
           ctrl_socket->local_endpoint().port(), endpoint_connected->address().to_string(),
           endpoint_connected->port());
  }

private:
  // order dependent
  io_context io_ctx;
  strand local_strand;
  steady_timer release_timer;
  shFX active_fx;
  Nanos latency;
  shZeroConfService zservice;
  uint8v work_buff;
  std::shared_ptr<work_guard> guard;

  // order independent
  Thread thread_main;
  Threads threads;
  std::stop_token stop_token;

  std::optional<udp_socket> data_socket;
  std::optional<udp_endpoint> data_endpoint;
  std::optional<tcp_socket> ctrl_socket;
  std::optional<tcp_endpoint> ctrl_endpoint;
  std::optional<tcp_endpoint> endpoint_connected;

  static constexpr csv module_id = "DESK";
};

} // namespace pierre
