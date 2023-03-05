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

#include "desk/dmx_ctrl.hpp"
#include "base/pet.hpp"
#include "base/thread_util.hpp"
#include "desk/async/read.hpp"
#include "desk/async/write.hpp"
#include "desk/msg/data.hpp"
#include "desk/msg/in.hpp"
#include "desk/msg/out.hpp"
#include "io/log_socket.hpp"
#include "lcs/config.hpp"
#include "lcs/config_watch.hpp"
#include "lcs/logger.hpp"
#include "lcs/stats.hpp"
#include "mdns/mdns.hpp"

#include <array>
#include <iterator>
#include <latch>
#include <utility>

namespace pierre {
namespace desk {

inline const string get_cfg_host() noexcept {
  return config_val<DmxCtrl, string>("remote.host"sv, "dmx");
}

inline auto stalled_timeout() noexcept {
  return config_val<DmxCtrl, Millis>("local.stalled.ms"sv, 7500);
}

// general API
DmxCtrl::DmxCtrl() noexcept
    : ctrl_sock(io_ctx),                                          // ctrl channel socket
      acceptor(io_ctx, tcp_endpoint{ip_tcp::v4(), ANY_PORT}),     // data channel listener
      data_sock(io_ctx),                                          // accepted data channel sock
      read_buff(2048),                                            //
      write_buff(2048),                                           //
      stalled_timer(io_ctx, stalled_timeout()),                   //
      thread_count(config_threads<DmxCtrl>(thread_count)),        //
      shutdown_latch(std::make_shared<std::latch>(thread_count)), //
      cfg_fut(ConfigWatch::want_changes()) // register for config change notifications
{
  INFO_INIT("sizeof={:>5} thread_count={}\n", sizeof(DmxCtrl), thread_count);

  // here we post resolve_host() to the io_context we're about to start
  //
  // resolve_host() may BLOCK or FAIL
  //  -if successful, handshake() is called to setup the control and data sockets
  //  -if failure, unknown_host() is invoked to retry resolution
  resolve_host();

  // NOTE: we do not start listening or the stalled timer at this point
  //       they are handled as needed during handshake() and listen()

  // start threads and io_ctx
  for (auto n = 0; n < thread_count; n++) {
    std::jthread([this, n = n]() mutable {
      const auto thread_name = thread_util::set_name(task_name, n);

      INFO_THREAD_START();
      io_ctx.run();

      shutdown_latch->count_down();
      INFO_THREAD_STOP();
    }).detach();
  }
}

DmxCtrl::~DmxCtrl() noexcept {
  INFO_SHUTDOWN_REQUESTED();

  io_ctx.stop();
  if (shutdown_latch) shutdown_latch->wait();

  INFO_SHUTDOWN_COMPLETE();
}

void DmxCtrl::handshake() noexcept {
  static constexpr csv fn_id{"handshake"};
  static constexpr csv idle_ms_path{"remote.idle_shutdown.ms"};
  static constexpr csv stats_ms_path{"remote.stats.ms"};

  // start listening on the data port we're preparing to send to
  // the remote host which will initiate a connection, upon receipt.
  listen();

  desk::MsgOut msg(HANDSHAKE);

  msg.add_kv(desk::IDLE_SHUTDOWN_MS, config_val<DmxCtrl, int64_t>(idle_ms_path, 10000));
  msg.add_kv(desk::STATS_MS, config_val<DmxCtrl, int64_t>(stats_ms_path, 2000));
  msg.add_kv(desk::REF_US, pet::now_monotonic<Micros>());
  msg.add_kv(desk::DATA_PORT, acceptor.local_endpoint().port());

  async::write_msg(ctrl_sock, write_buff, std::move(msg), [this](MsgOut msg) {
    Stats::write(stats::CTRL_MSG_WRITE_ELAPSED, msg.elapsed());

    if (msg.xfer_error()) {
      Stats::write(stats::CTRL_MSG_WRITE_ERROR, true);

      INFO_AUTO("write failed, reason={}\n", msg.ec.message());
    }
  });
}

void DmxCtrl::listen() noexcept {
  static constexpr csv fn_id{"listen"};
  // listen for inbound data connections and get our local port
  // when this async_accept is successful msg_loop() is invoked
  acceptor.async_accept( //
      data_sock, [this, e = Elapsed()](const error_code ec) mutable {
        Stats::write(stats::DATA_CONNECT_ELAPSED, e.freeze());

        if (!ec) {
          const auto &r = data_sock.remote_endpoint();
          INFO_AUTO("{}\n", io::log_socket_msg(ec, data_sock, r, e));

          // success, set sock opts, connected and start msg_loop()
          data_sock.set_option(ip_tcp::no_delay(true));
          connected = ctrl_sock.is_open() && data_sock.is_open();

          msg_loop(); // start the msg loop

        } else {
          INFO_AUTO("ctrl={} data={} {}\n", ctrl_sock.is_open(), data_sock.is_open(), ec.message());
        }
      });
}

void DmxCtrl::msg_loop() noexcept {
  // only attempt to read a message if we're connected
  if (connected) {
    async::read_msg(ctrl_sock, read_buff, desk::MsgIn(), [this](desk::MsgIn msg) {
      Stats::write(stats::CTRL_MSG_READ_ELAPSED, msg.elapsed());

      DynamicJsonDocument doc(desk::Msg::default_doc_size);
      msg.deserialize_from(read_buff, doc);

      if (msg.xfer_ok()) {
        stalled_watchdog(); // restart stalled watchdog

        // handle the various message types
        if (msg.is_msg_type(doc, desk::FEEDBACK)) {
          const auto echo_now_us = doc["echo_now_µs"].as<int64_t>();
          Stats::write(stats::REMOTE_ROUNDTRIP, pet::elapsed_from_raw<Micros>(echo_now_us));
          Stats::write(stats::REMOTE_DATA_WAIT, Micros(doc["data_wait_µs"] | 0));
          Stats::write(stats::REMOTE_ELAPSED, Micros(doc["elapsed_µs"] | 0));

        } else if (msg.is_msg_type(doc, desk::STATS)) {
          Stats::write(stats::REMOTE_DMX_QOK, doc["dmx_qok"].as<int64_t>());
          Stats::write(stats::REMOTE_DMX_QRF, doc["dmx_qrf"].as<int64_t>());
          Stats::write(stats::REMOTE_DMX_QSF, doc["dmx_qsf"].as<int64_t>());
          Stats::write(stats::FPS, doc["fps"].as<int64_t>());
        }

        msg_loop(); // async wait for next message

      } else {
        Stats::write(stats::CTRL_MSG_READ_ERROR, true);
      }
    });
  }
}

void DmxCtrl::resolve_host() noexcept {
  static constexpr csv fn_id{"host.resolve"};

  asio::post(io_ctx, [this]() {
    // get the host we will connect to unless we already have it
    if (cfg_host.empty()) cfg_host = get_cfg_host();

    INFO_AUTO("attempting to resolve '{}'\n", cfg_host);

    // start name resolution via mdns
    auto zcsf = mDNS::zservice(cfg_host);

    if (auto zcs_status = zcsf.wait_for(15s); zcs_status != std::future_status::ready) {
      INFO_AUTO("resolution timeout for '{}'\n", cfg_host);
      unknown_host();
      return;
    }

    auto zcs = zcsf.get();

    INFO_AUTO("resolution attempt complete, valid={}\n", zcs.valid());

    // resolution failed, enter unknown host processing
    if (!zcs.valid()) {
      unknown_host();
      return;
    }

    // ok, we've resolved the host.  now we start the watchdog to catch
    // connection timeouts
    stalled_watchdog();

    // we'll connect to this endpoint
    host_endpoint.emplace(asio::ip::make_address_v4(zcs.address()), zcs.port());

    // kick-off an async connect. once connected we'll send the handshake
    // that contains the data port for the remote host to connect
    asio::async_connect( //
        ctrl_sock, std::array{host_endpoint.value()},
        [this, e = Elapsed()](const error_code ec, const tcp_endpoint r) mutable {
          static constexpr csv fn_id{"host.connect"};

          INFO_AUTO("{}\n", io::log_socket_msg(ec, ctrl_sock, r, e));

          if (ec) return;

          ctrl_sock.set_option(ip_tcp::no_delay(true));
          Stats::write(stats::CTRL_CONNECT_ELAPSED, e.freeze());

          handshake();
        });
  });
}

void DmxCtrl::send_data_msg(DataMsg &&msg) noexcept {
  if (connected) { // only send msgs when connected

    async::write_msg(data_sock, write_buff, std::move(msg), [this](MsgOut msg) {
      Stats::write(stats::DATA_MSG_WRITE_ELAPSED, msg.elapsed());

      if (msg.xfer_error()) {
        Stats::write(stats::DATA_MSG_WRITE_ERROR, true);
        connected.store(false);
      }
    });
  }
}

void DmxCtrl::stalled_watchdog(Millis wait) noexcept {
  static constexpr csv fn_id{"stalled"};

  // when passed a non-zero wait time we're handling a special
  // case (e.g. host resolve failure), otherwise use configured
  // stall timeout value
  if (wait == Millis::zero()) wait = stalled_timeout();

  if (ConfigWatch::has_changed(cfg_fut) && (cfg_host != get_cfg_host())) {
    cfg_host.clear();                      // triggers pull from config on next name resolution
    cfg_fut = ConfigWatch::want_changes(); // get a fresh future
    wait = 0s;                             // override wait, config has changed
  }

  stalled_timer.expires_after(wait);
  stalled_timer.async_wait([this](error_code ec) {
    if (ec == errc::success) {

      if (const auto was_connected = std::atomic_exchange(&connected, false)) {
        INFO_AUTO("was_connected={}\n", was_connected);
      }

      acceptor.close(ec);
      ctrl_sock.close(ec);
      data_sock.close(ec);

      resolve_host(); // kick off name resolution, connect sequence

    } else if (ec != errc::operation_canceled) {
      INFO_AUTO("falling through {}\n", ec.message());
    }
  });
}

void DmxCtrl::unknown_host() noexcept {
  static constexpr csv fn_id{"host.unknown"};

  INFO_AUTO("{} not found, next attempt in 60s\n", cfg_host);

  cfg_host.clear();
  stalled_watchdog(60s);
}

} // namespace desk
} // namespace pierre
