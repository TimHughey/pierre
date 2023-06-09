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
#include "desk/fdecls.hpp"

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

class DmxCtrl {
public:
  /// @brief  Create and initialize DmxCtrl.
  ///         The constructor does not start the threads or connection sequence.
  ///         See resume() and handshake() for more details.
  DmxCtrl() noexcept;
  ~DmxCtrl() noexcept;

  void resume() noexcept;

  /// @brief send DataMsg to DMX host
  /// @param msg DataMsg for DMX host
  void send_data_msg(DataMsg &&msg) noexcept;

private:
  auto cfg_host() noexcept { return tokc->val<string>("remote.host", "dmx"); }
  auto cfg_stall_timeout() noexcept { return tokc->val<Millis>("local.stalled.ms", 7'333); }

  /// @brief Send the initial handshake to start the session with
  ///        the DMX host.
  ///        The data connection is opened once the handshake is complete
  ///        and data msgs are transmitted. This officially begins
  ///        rendering of the audio peaks as colors.
  void handshake() noexcept;

  /// @brief Responsible for handling session msgs which consist primarily
  ///        of DMX host rendering messaeges.
  /// @param msg Reeived session message
  void msg_loop(MsgIn &&msg) noexcept;

  /// @brief Load configuration from conf::token at startup and when
  ///        the configuration changes on-disk (as notified by watch).
  ///        If the configuration changes the DMX host the previous host
  ///        is disconnected and a connection is established to the
  ///        new DMX host.
  void load_config() noexcept;

  /// @brief Reconnect the DMX host.  This member function is used
  ///        when a stall is detected or when the configuration changes.
  void reconnect() noexcept {
    asio::dispatch(sess_strand, [this]() { stall_watchdog(0ms); });
  }

  /// @brief Perform mDNS resolution for configured DMX host.
  ///        NOTE: this function may block during resolution
  ///        so this must be accounted for in the number of
  ///        threads for DmxCrtl.
  void resolve_host() noexcept;

  /// @brief Perform a DNS reverse resolution of the DMX host IP
  ///        returned by resolve_host()
  /// @param ep IP address endpoint to resolve
  void rev_resolve(tcp_endpoint ep) noexcept;

  /// @brief Schedule (or reschedule) the stall watchdog with the
  ///        default stall timeout.
  ///        When the stall watchdog expires reconnect() invoked.
  void stall_watchdog() noexcept { stall_watchdog(stall_timeout); }

  /// @brief Schedule the stall watchdog with a specifc wait duration.
  ///        This overload is used to force an immediate reconnect
  ///        when the configuration changes.
  /// @param wait Duration to wait before timer fires
  void stall_watchdog(Millis wait) noexcept;

  /// @brief Start threads
  void threads_start() noexcept;

  /// @brief Special handling in the event the configured DMX host
  ///        can not be resolved via mDNS.
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
  MOD_ID("desk.dmx_ctrl");
};

} // namespace desk
} // namespace pierre
