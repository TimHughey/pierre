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
  ~Ctrl() noexcept;

  static auto create(io_context &io_ctx) noexcept {
    return std::shared_ptr<Ctrl>(new Ctrl(io_ctx));
  }

  auto ptr() noexcept { return shared_from_this(); }

  std::shared_ptr<Ctrl> init() noexcept;

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

  // wait for handshake message from remote endpoint
  void handshake() noexcept;

  void listen() noexcept;

  void msg_loop() noexcept;

  void send_ctrl_msg(io::Msg msg) noexcept;
  void stalled_watchdog() noexcept;

  // misc debug
  const error_code log_accept(const error_code ec, Elapsed e) noexcept;
  const error_code log_connect(const error_code ec, Elapsed &e, const tcp_endpoint &r) noexcept;
  void log_feedback(JsonDocument &doc) noexcept;
  void log_handshake(JsonDocument &doc);
  const error_code log_read_msg(const error_code ec, Elapsed e) noexcept;
  const error_code log_send_ctrl_msg(const error_code ec, const size_t tx_want,
                                     const size_t tx_actual, Elapsed e) noexcept;
  const error_code log_send_data_msg(const error_code ec, Elapsed e) noexcept;

private:
  // order dependent
  io_context &io_ctx;
  tcp_acceptor acceptor;
  steady_timer stalled_timer;

  // order independent
  std::atomic_bool connected{false};
  std::atomic_bool connecting{false};
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