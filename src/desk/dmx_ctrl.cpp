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

#include "dmx_ctrl.hpp"
#include "async_msg.hpp"
#include "base/thread_util.hpp"
#include "base/uint8v.hpp"
#include "lcs/config.hpp"
#include "lcs/config_watch.hpp"
#include "lcs/logger.hpp"
#include "lcs/stats.hpp"
#include "mdns/mdns.hpp"
#include "msg.hpp"

#include <array>
#include <iterator>
#include <latch>
#include <ranges>
#include <utility>

namespace pierre {

static const string get_cfg_host() noexcept {
  return config_val2<DmxCtrl, string>("remote.host"sv, "dmx");
}

static auto stalled_timeout() noexcept {
  const auto ms = config_val2<DmxCtrl, int64_t>("local.stalled.ms"sv, 7500);

  return pet::from_val<Nanos, Millis>(ms);
}

// general API
DmxCtrl::DmxCtrl() noexcept
    : ctrl_sock(io_ctx),                                          // ctrl channel socket
      acceptor(io_ctx, tcp_endpoint{ip_tcp::v4(), ANY_PORT}),     // data channel listener
      data_sock(io_ctx),                                          // accepted data channel sock
      stalled_timer(io_ctx, stalled_timeout()),                   //
      thread_count(config_threads<DmxCtrl>(2)),                   //
      startup_latch(std::make_unique<std::latch>(thread_count)),  //
      shutdown_latch(std::make_unique<std::latch>(thread_count)), //
      cfg_fut(ConfigWatch::want_changes()) // register for config change notifications
{
  INFO_INIT("sizeof={:>5} thread_count={}\n", sizeof(DmxCtrl), thread_count);

  // add work to thje io_context we're about to start, specificially name resolution
  // for the dmx host
  //
  // resolve_host() may BLOCK or FAIL
  //  -if successful, connect() is invoked to the resolved host
  //  -if failure, unknown_host() is invoked to retry resolution
  resolve_host();

  // NOTE: we do not start listening or the stalled timer at this point
  //       they are handled as needed during the connect(), listen() and handshake()

  // start threads and io_ctx
  for (auto n = 0; n < thread_count; n++) {
    std::jthread([this, n = n]() mutable {
      const auto thread_name = thread_util::set_name(task_name, n);

      startup_latch->count_down();
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

void DmxCtrl::connect() noexcept {
  static constexpr csv cat{"ctrl_sock"};

  // detect and handle connection timeout
  stalled_watchdog();

  asio::async_connect( //
      ctrl_sock,       //
      std::array{host_endpoint.value()},
      [this, e = Elapsed()](const error_code ec, const tcp_endpoint r) mutable {
        if (!ec) {
          static constexpr csv idle_ms_path{"remote.idle_shutdown.ms"};
          static constexpr csv stats_ms_path{"remote.stats.ms"};
          INFO(module_id, cat, "{}\n", io::log_socket_msg(ec, ctrl_sock, r, e));

          ctrl_sock.set_option(ip_tcp::no_delay(true));
          Stats::write(stats::CTRL_CONNECT_ELAPSED, e.freeze());

          // listen for an incomming connection from the dmx controller as soon as
          // we send the first ctrl msg
          listen();

          desk::Msg msg(HANDSHAKE);

          msg.add_kv("idle_shutdown_ms", config_val2<DmxCtrl, int64_t>(idle_ms_path, 10000));
          msg.add_kv("stats_ms", config_val2<DmxCtrl, int64_t>(stats_ms_path, 2000));
          msg.add_kv("lead_time_µs", InputInfo::lead_time);
          msg.add_kv("ref_µs", pet::now_monotonic<Micros>());
          msg.add_kv("data_port", acceptor.local_endpoint().port());

          send_ctrl_msg(std::move(msg));
        }
      });

  // NOTE: intentionally fall through
  //       listen() will start msg_loop() when data connection is available
}

void DmxCtrl::handle_feedback_msg(JsonDocument &doc) noexcept {
  const int64_t echo_now_us = doc["echo_now_µs"].as<int64_t>();
  const auto roundtrip = pet::elapsed_from_raw<Micros>(echo_now_us);
  Stats::write(stats::REMOTE_ROUNDTRIP, roundtrip);

  Stats::write(stats::REMOTE_DATA_WAIT, Micros(doc["data_wait_µs"] | 0));
  Stats::write(stats::REMOTE_ELAPSED, Micros(doc["elapsed_µs"] | 0));
  Stats::write(stats::REMOTE_DMX_QOK, doc["dmx_qok"].as<int64_t>());
  Stats::write(stats::REMOTE_DMX_QRF, doc["dmx_qrf"].as<int64_t>());
  Stats::write(stats::REMOTE_DMX_QSF, doc["dmx_qsf"].as<int64_t>());

  const int64_t fps = doc["fps"].as<int64_t>();
  Stats::write(stats::FPS, fps);
}

void DmxCtrl::listen() noexcept {
  static constexpr csv cat{"data_sock"};
  // listen for inbound data connections and get our local port
  // when this async_accept is successful msg_loop() is invoked
  acceptor.async_accept( //
      data_sock,         //
      [this, e = Elapsed()](const error_code ec) mutable {
        Stats::write(stats::DATA_CONNECT_ELAPSED, e.freeze());

        if (!ec) {

          const auto &r = data_sock.remote_endpoint();
          INFO(module_id, cat, "{}\n", io::log_socket_msg(ec, data_sock, r, e));

          // success, set sock opts, connected and start msg_loop()
          data_sock.set_option(ip_tcp::no_delay(true));

          connected = ctrl_sock.is_open() && data_sock.is_open();

          msg_loop(); // start the msg loop

        } else {
          INFO(module_id, cat, "ctrl_socket={} data_socket={} {}\n", //
               ctrl_sock.is_open(), data_sock.is_open(), ec.message());
        }
      });
}

void DmxCtrl::msg_loop() noexcept {
  // only attempt to read a message if we're connected
  if (connected) {
    desk::async_read_msg( //
        ctrl_sock, [this, e = Elapsed()](const error_code ec, desk::Msg msg) mutable {
          Stats::write(stats::CTRL_MSG_READ_ELAPSED, e.freeze());

          if (ec == errc::success) {
            stalled_watchdog(); // restart stalled watchdog
            // handle the various message types
            if (msg.key_equal(desk::TYPE, FEEDBACK)) handle_feedback_msg(msg.doc);

            msg_loop(); // async handle next message

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

    if (zcs.valid()) {
      // create the endpoint we'll connect
      host_endpoint.emplace(asio::ip::make_address_v4(zcs.address()), zcs.port());

      // kick-off connect sequence and handshake
      connect();
    } else {
      // resolution failed, kick-off timer to retry
      unknown_host();
      return;
    }
  });
}

void DmxCtrl::send_ctrl_msg(desk::Msg &&msg) noexcept {
  static constexpr csv fn_id{"send_ctrl_msg"};

  msg.serialize();
  const auto buf_seq = msg.buff_seq(); // calculates tx_len
  const auto tx_len = msg.tx_len;

  asio::async_write( //
      ctrl_sock,     //
      buf_seq,       //
      asio::transfer_exactly(tx_len),
      [this, msg = std::move(msg), e = Elapsed()](const error_code ec, const auto actual) mutable {
        Stats::write(stats::CTRL_MSG_WRITE_ELAPSED, e.freeze());

        if ((ec != errc::success) && (actual != msg.tx_len)) {
          Stats::write(stats::CTRL_MSG_WRITE_ERROR, true);

          INFO(module_id, fn_id, "write failed, reason={} bytes={}/{}\n", //
               ec.message(), actual, msg.tx_len);
        }
      });
}

void DmxCtrl::send_data_msg(DmxDataMsg msg) noexcept {
  if (connected) { // only send msgs when connected

    msg.finalize();

    desk::async_write_msg( //
        data_sock,         //
        std::move(msg), [this, e = Elapsed()](const error_code ec) mutable {
          // better readability

          Stats::write(stats::DATA_MSG_WRITE_ELAPSED, e.freeze());

          if (ec != errc::success) {
            Stats::write(stats::DATA_MSG_WRITE_ERROR, true);
            connected.store(false);
          }

          return ec;
        });
  }
}

void DmxCtrl::stalled_watchdog(Nanos wait) noexcept {
  static constexpr csv fn_id{"stalled"};

  // when passed a non-zero wait time we're handling a special
  // case (e.g. host resolve failure), otherwise use configured
  // stall timeout value
  if (wait == Nanos::zero()) wait = stalled_timeout();

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

      acceptor.cancel(ec);
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

} // namespace pierre
