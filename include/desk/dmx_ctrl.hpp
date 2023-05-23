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

#include "base/asio.hpp"
#include "base/conf/token.hpp"
#include "base/dura_t.hpp"
#include "base/types.hpp"

#include <atomic>
#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system.hpp>
#include <memory>
#include <optional>
#include <streambuf>
#include <thread>

namespace pierre {

using strand_ioc = asio::strand<asio::io_context::executor_type>;
using steady_timer = asio::steady_timer;
using ip_address = boost::asio::ip::address;
using ip_tcp = boost::asio::ip::tcp;

using tcp_acceptor = boost::asio::ip::tcp::acceptor;
using tcp_endpoint = boost::asio::ip::tcp::endpoint;
using tcp_socket = boost::asio::ip::tcp::socket;
using tcp_resolver = ip_tcp::resolver;
using resolve_results = ip_tcp::resolver::results_type;

namespace desk {

// forward decl
class DataMsg;
class MsgIn;

class DmxCtrl {
public:
  DmxCtrl() noexcept;
  ~DmxCtrl() noexcept;

  void resume() noexcept;

  /// @brief send the DataMsg to the remote host
  /// @param msg assembled DataMsg for remote host
  void send_data_msg(DataMsg &&msg) noexcept;

private:
  auto cfg_host() noexcept { return tokc->val<string>("remote.host"_tpath, "dmx"sv); }
  auto cfg_stall_timeout() noexcept { return tokc->val<Millis>("local.stalled.ms"_tpath, 7500); }

  /// @brief send the initial handshake to the remote host
  void handshake() noexcept;

  void msg_loop(MsgIn &&msg) noexcept;
  void load_config() noexcept;

  void reconnect() noexcept {
    asio::dispatch(sess_strand, [this]() { stall_watchdog(0ms); });
  }

  void resolve_host() noexcept;

  void rev_resolve(tcp_endpoint ep) noexcept;

  void stall_watchdog() noexcept { stall_watchdog(stall_timeout); }
  void stall_watchdog(Millis wait) noexcept;
  void threads_start() noexcept;
  void unknown_host() noexcept;

private:
  // order independent
  conf::token *tokc;

  // order dependent
  std::vector<std::jthread> threads;
  asio::io_context io_ctx;
  strand_ioc sess_strand;
  strand_ioc data_strand;
  tcp_endpoint data_lep{ip_tcp::v4(), ANY_PORT};
  tcp_socket sess_sock; // handshake, stats (read/write)
  tcp_acceptor data_accep;
  tcp_socket data_sock; // frame data (write only)
  steady_timer stalled_timer;
  steady_timer resolve_retry_timer;
  std::unique_ptr<tcp_resolver> resolver;

  // order independent
  std::atomic_flag sess_connected;
  std::atomic_flag data_connected;
  Millis stall_timeout;

  tcp_endpoint data_rep; // remote endpoint of accepted socket

  string remote_host;                        // dmx controller host name
  std::optional<tcp_endpoint> host_endpoint; // resolved dmx controller endpoint

  // misc debug
public:
  static constexpr csv module_id{"desk.dmx_ctrl"};
};

} // namespace desk
} // namespace pierre
