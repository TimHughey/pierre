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

#include "base/pet_types.hpp"
#include "base/types.hpp"
#include "io/buffer.hpp"
#include "io/context.hpp"
#include "io/error.hpp"
#include "io/tcp.hpp"
#include "io/timer.hpp"
#include "lcs/types.hpp"

#include <atomic>
#include <future>
#include <latch>
#include <memory>
#include <optional>
#include <streambuf>
#include <thread>

namespace pierre {
namespace desk {

// forward decl
class DataMsg;
class MsgIn;

class DmxCtrl {
public:
  DmxCtrl() noexcept;
  ~DmxCtrl() noexcept;

  /// @brief send the DataMsg to the remote host
  /// @param msg assembled DataMsg for remote host
  void send_data_msg(DataMsg &&msg) noexcept;

private:
  /// @brief send the initial handshake to the remote host
  void handshake() noexcept;

  void msg_loop(MsgIn &&msg) noexcept;

  void resolve_host() noexcept;
  void stall_watchdog(Millis wait = Millis::zero()) noexcept;
  void stall_watchdog_cancel() noexcept;
  void unknown_host() noexcept;

private:
  // order dependent
  io_context io_ctx;
  tcp_socket data_sock;
  steady_timer stalled_timer;
  steady_timer resolve_retry_timer;
  const int64_t thread_count;
  std::shared_ptr<std::latch> shutdown_latch;
  cfg_future cfg_fut;

  // order independent
  std::atomic_bool connected{false};

  string cfg_host;                           // dmx controller host name
  std::optional<tcp_endpoint> host_endpoint; // resolved dmx controller endpoint

  // misc debug
public:
  static constexpr csv module_id{"desk.dmx_ctrl"};
  static constexpr csv task_name{"dmx_ctrl"};
};

} // namespace desk
} // namespace pierre
