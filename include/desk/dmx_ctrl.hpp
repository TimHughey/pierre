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
#include "base/pet.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "desk/dmx_data_msg.hpp"
#include "desk/msg.hpp"
#include "io/io.hpp"

#include <atomic>
#include <future>
#include <latch>
#include <memory>
#include <optional>
#include <thread>

namespace pierre {

class DmxCtrl {
public:
  DmxCtrl() noexcept;
  ~DmxCtrl() noexcept;

  bool ready() noexcept {
    return ctrl_sock.has_value() && //
           data_sock.has_value() && //
           ctrl_sock->is_open() &&  //
           data_sock->is_open();
  }

  void run() noexcept;

  void send_data_msg(DmxDataMsg msg) noexcept;

private:
  // lookup dmx controller and establish control connection
  void connect() noexcept;

  // handle received feedback msgs
  void handle_feedback_msg(JsonDocument &doc) noexcept;

  void listen() noexcept;

  void msg_loop() noexcept;

  void send_ctrl_msg(desk::Msg msg) noexcept;
  void stalled_watchdog() noexcept;

private:
  // order dependent
  io_context io_ctx;
  tcp_acceptor acceptor;
  strand stall_strand;
  steady_timer stalled_timer;
  const int64_t thread_count;
  std::unique_ptr<std::latch> startup_latch;
  std::unique_ptr<std::latch> shutdown_latch;

  // order independent
  std::atomic_bool connected{false};
  std::optional<tcp_socket> ctrl_sock;
  std::optional<tcp_socket> data_sock;

  // ctrl message types
  static constexpr csv FEEDBACK{"feedback"};
  static constexpr csv HANDSHAKE{"handshake"};

  // misc debug
public:
  static constexpr csv module_id{"desk.dmx_ctrl"};
  static constexpr csv task_name{"dmx_ctrl"};
};

} // namespace pierre
