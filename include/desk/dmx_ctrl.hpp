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
#include "base/pet_types.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "desk/msg.hpp"
#include "io/context.hpp"
#include "io/tcp.hpp"
#include "io/timer.hpp"
#include "lcs/types.hpp"

#include <atomic>
#include <future>
#include <latch>
#include <memory>
#include <optional>
#include <thread>

namespace pierre {
namespace desk {

class DmxCtrl {
public:
  DmxCtrl() noexcept;
  ~DmxCtrl() noexcept;

  void send_data_msg(desk::Msg &&msg) noexcept;

private:
  // lookup dmx controller and establish control connection
  void connect() noexcept;

  // handle received feedback msgs
  void handle_feedback_msg(JsonDocument &doc) noexcept;

  void listen() noexcept;

  void msg_loop() noexcept;

  void resolve_host() noexcept;
  void send_ctrl_msg(desk::Msg &&msg) noexcept;
  void stalled_watchdog(Millis wait = Millis()) noexcept;
  void unknown_host() noexcept;

private:
  // order dependent
  io_context io_ctx;
  tcp_socket ctrl_sock;
  tcp_acceptor acceptor;
  tcp_socket data_sock;
  steady_timer stalled_timer;
  const int64_t thread_count;
  std::unique_ptr<std::latch> startup_latch;
  std::unique_ptr<std::latch> shutdown_latch;
  cfg_future cfg_fut;

  // order independent
  std::atomic_bool connected{false};

  string cfg_host;                           // dmx controller host name
  std::optional<tcp_endpoint> host_endpoint; // resolved dmx controller endpoint

  // ctrl message types
  static constexpr csv FEEDBACK{"feedback"};
  static constexpr csv HANDSHAKE{"handshake"};

  // misc debug
public:
  static constexpr csv module_id{"desk.dmx_ctrl"};
  static constexpr csv task_name{"dmx_ctrl"};
};

} // namespace desk
} // namespace pierre
