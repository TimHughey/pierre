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
#include "lcs/types.hpp"

#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/system.hpp>
#include <memory>
#include <optional>
#include <streambuf>
#include <thread>

namespace pierre {

namespace asio = boost::asio;
namespace sys = boost::system;
namespace errc = boost::system::errc;

using error_code = boost::system::error_code;
using strand_tp = asio::strand<asio::thread_pool::executor_type>;
using steady_timer = asio::steady_timer;
using work_guard_tp = asio::executor_work_guard<asio::thread_pool::executor_type>;
using ip_address = boost::asio::ip::address;
using ip_tcp = boost::asio::ip::tcp;
using tcp_acceptor = boost::asio::ip::tcp::acceptor;
using tcp_endpoint = boost::asio::ip::tcp::endpoint;
using tcp_socket = boost::asio::ip::tcp::socket;

namespace desk {

// forward decl
class DataMsg;
class MsgIn;

class DmxCtrl {
public:
  DmxCtrl() noexcept;
  ~DmxCtrl() noexcept {
    thread_pool.stop();
    thread_pool.join();
  }

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
  const int64_t thread_count;
  asio::thread_pool thread_pool;
  tcp_endpoint data_lep{ip_tcp::v4(), ANY_PORT};
  tcp_socket sess_sock; // handshake, stats (read/write)
  tcp_acceptor data_accep;
  tcp_socket data_sock; // frame data (write only)

  Millis stall_timeout;
  steady_timer stalled_timer;
  steady_timer resolve_retry_timer;

  cfg_future cfg_fut;

  // order independent
  tcp_endpoint data_rep; // remote endpoint of accepted socket
  std::atomic_bool connected{false};
  std::atomic_bool data_connected{false};

  string cfg_host;                           // dmx controller host name
  std::optional<tcp_endpoint> host_endpoint; // resolved dmx controller endpoint

  // misc debug
public:
  static constexpr csv module_id{"desk.dmx_ctrl"};
  static constexpr csv task_name{"dmx_ctrl"};
};

} // namespace desk
} // namespace pierre
