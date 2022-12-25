// Pierre
// Copyright (C) 2022 Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#pragma once

#include "base/elapsed.hpp"
#include "base/io.hpp"
#include "base/logger.hpp"
#include "base/pet.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "desk/dmx_data_msg.hpp"
#include "io/msg.hpp"

#include <atomic>
#include <future>
#include <memory>
#include <optional>

namespace pierre {

class DmxCtrl : public std::enable_shared_from_this<DmxCtrl> {
private:
  DmxCtrl(io_context &io_ctx) noexcept;

public:
  static auto create(io_context &io_ctx) noexcept {
    return std::shared_ptr<DmxCtrl>(new DmxCtrl(io_ctx));
  }

  auto ptr() noexcept { return shared_from_this(); }

  auto init() noexcept {
    // start stalled_watchdog() to detect:
    //  1. zeroconf resolution timeouts
    //  2. unable to make initial handshake connection
    //  3. failure by remote controller to make inbound data connection
    //  4. failure to receive regular ctrl feedbacks

    stalled_watchdog();

    listen();
    connect();

    return shared_from_this();
  }

  bool ready() noexcept {
    return ctrl_sock.has_value() && //
           data_sock.has_value() && //
           ctrl_sock->is_open() &&  //
           data_sock->is_open();
  }

  void send_data_msg(DmxDataMsg msg) noexcept;

  void teardown() noexcept {

    asio::post(io_ctx, [s = ptr()]() mutable {
      [[maybe_unused]] error_code ec;

      s->stalled_timer.cancel(ec);
      s->acceptor.cancel(ec);
      if (s->ctrl_sock.has_value()) s->ctrl_sock->cancel(ec);
      if (s->data_sock.has_value()) s->data_sock->cancel(ec);
    });
  }

private:
  // lookup dmx controller and establish control connection
  void connect() noexcept;

  // handle received feedback msgs
  void handle_feedback_msg(JsonDocument &doc) noexcept;

  void listen() noexcept;

  void msg_loop() noexcept;

  void send_ctrl_msg(io::Msg msg) noexcept;
  void stalled_watchdog() noexcept;

private:
  // order dependent
  io_context &io_ctx;
  strand local_strand;
  tcp_acceptor acceptor;
  steady_timer stalled_timer;

  // order independent
  std::atomic_bool connected{false};
  std::optional<tcp_socket> ctrl_sock;
  std::optional<tcp_socket> data_sock;

  // ctrl message types
  static constexpr csv FEEDBACK{"feedback"};
  static constexpr csv HANDSHAKE{"handshake"};

  // misc debug
public:
  static constexpr csv module_id{"DMX_CTRL"};
};

} // namespace pierre
