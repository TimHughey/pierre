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
#include "desk/data_msg.hpp"
#include "io/msg.hpp"

#include <atomic>
#include <future>
#include <memory>
#include <optional>

namespace pierre {
namespace desk {

using ctrl_fut_ec_t = std::future<error_code>;

class Ctrl : public std::enable_shared_from_this<Ctrl> {
private:
  Ctrl(io_context &io_ctx) noexcept;

public:
  ~Ctrl() = default;

  static auto create(io_context &io_ctx) noexcept {
    return std::shared_ptr<Ctrl>(new Ctrl(io_ctx));
  }

  auto ptr() noexcept { return shared_from_this(); }

  std::shared_ptr<Ctrl> init() noexcept {
    // NOTE:  stalled_watchdog will be started as needed

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

  void send_data_msg(DataMsg data_msg) noexcept;

private:
  // lookup dmx controller and establish control connection
  void connect() noexcept;

  // handle received feedback msgs
  void handle_feedback_msg(JsonDocument &doc) noexcept;

  // wait for handshake message from remote endpoint
  void handshake() noexcept;

  void listen() noexcept;

  void msg_loop() noexcept;

  void reconnect_if_needed(const error_code ec) noexcept;

  void send_ctrl_msg(io::Msg msg) noexcept;
  void stalled_watchdog() noexcept;

  // misc debug
  error_code log_socket(csv type, error_code ec, tcp_socket &sock, Elapsed &e) noexcept {
    return log_socket(type, ec, sock, sock.remote_endpoint(), e);
  }

  error_code log_socket(csv type, error_code ec, tcp_socket &sock, const tcp_endpoint &r,
                        Elapsed &e) noexcept;

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
  static constexpr csv module_id{"DESK_CONTROL"};
};

} // namespace desk
} // namespace pierre