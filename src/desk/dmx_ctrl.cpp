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
#include "base/threads.hpp"
#include "base/uint8v.hpp"
#include "lcs/config.hpp"
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

static auto cfg_path(csv key_path) noexcept {
  return toml::path(DmxCtrl::module_id).append(key_path);
}

static const auto controller_name() noexcept {
  return config()->at(cfg_path("controller")).value_or<string>("dmx");
}

static const auto idle_shutdown() noexcept {
  return pet::from_val<Nanos, Millis>(
      config()->at(cfg_path("timeouts.milliseconds.idle")).value_or(30000));
}

static const auto stalled_timeout() noexcept {
  static constexpr csv PATH{"timeouts.milliseconds.stalled"};

  return pet::from_val<Nanos, Millis>(config()->at(PATH).value_or(7500));
}

static const auto threads_path() noexcept { return cfg_path("threads"); }

// general API
DmxCtrl::DmxCtrl() noexcept
    : acceptor(io_ctx, tcp_endpoint{ip_tcp::v4(), ANY_PORT}),                  //
      stall_strand(io_ctx),                                                    //
      stalled_timer(stall_strand.context().get_executor(), stalled_timeout()), //
      thread_count(config()->at(threads_path()).value_or(2)),                  //
      startup_latch(std::make_shared<std::latch>(thread_count)),               //
      shutdown_latch(std::make_shared<std::latch>(thread_count))               //
{
  static constexpr csv fn_id{"init"};

  INFO(module_id, "construct", "initiated\n");

  // post work for the io_ctx (also serves as guard)
  asio::post(io_ctx, [this, startup_latch = startup_latch]() mutable {
    stalled_watchdog();
    startup_latch->wait();
    startup_latch.reset();

    listen();
    connect();
  });

  for (auto n = 0; n < thread_count; n++) {
    std::jthread([this, n = n, startup_latch = startup_latch]() mutable {
      const auto thread_name = name_thread(task_name, n);
      INFO(module_id, fn_id, "thread {}\n", thread_name);

      startup_latch->arrive_and_wait();
      io_ctx.run();

      INFO(module_id, "shutdown", "thread {}\n", thread_name);
      shutdown_latch->count_down();
    }).detach();
  }

  startup_latch.reset();

  INFO(module_id, fn_id, "complete\n");
}

DmxCtrl::~DmxCtrl() noexcept {
  static constexpr csv fn_id("teardown");

  INFO(module_id, fn_id, "initiated\n");

  io_ctx.stop();
  shutdown_latch->wait();

  INFO(module_id, fn_id, "complete\n");
}

void DmxCtrl::connect() noexcept {
  static constexpr csv cat{"ctrl_sock"};

  // now begin the control channel connect and handshake
  auto zcsf = mDNS::zservice(controller_name()); // zerconf resolve future
  Elapsed e;
  auto zcs = zcsf.get(); // can BLOCK, as needed

  const auto addr = asio::ip::make_address_v4(zcs.address());

  const std::array endpoints{tcp_endpoint{addr, zcs.port()}};
  asio::async_connect(           //
      ctrl_sock.emplace(io_ctx), //
      endpoints,                 //
      [this, e](const error_code ec, const tcp_endpoint r) mutable {
        if (ctrl_sock.has_value()) {

          INFO(module_id, cat, "{}\n", io::log_socket_msg(ec, *ctrl_sock, r, e));

          if (!ec) {

            ctrl_sock->set_option(ip_tcp::no_delay(true));
            Stats::write(stats::CTRL_CONNECT_ELAPSED, e.freeze());

            desk::Msg msg(HANDSHAKE);

            msg.add_kv("idle_shutdown_ms", idle_shutdown());
            msg.add_kv("lead_time_µs", InputInfo::lead_time);
            msg.add_kv("ref_µs", pet::now_realtime<Micros>());
            msg.add_kv("data_port", acceptor.local_endpoint().port());

            send_ctrl_msg(std::move(msg));
          }

        } else {
          INFO(module_id, cat, "no socket, {}\n", ec.message());
        }
      });

  // NOTE: intentionally fall through
  //       listen() will start msg_loop() when data connection is available
}

void DmxCtrl::handle_feedback_msg(JsonDocument &doc) noexcept {
  Stats::write(stats::REMOTE_DATA_WAIT, Micros(doc["data_wait_µs"] | 0));
  Stats::write(stats::REMOTE_ELAPSED, Micros(doc["elapsed_µs"] | 0));
  Stats::write(stats::REMOTE_DMX_QOK, doc["dmx_qok"].as<int64_t>());
  Stats::write(stats::REMOTE_DMX_QRF, doc["dmx_qrf"].as<int64_t>());
  Stats::write(stats::REMOTE_DMX_QSF, doc["dmx_qsf"].as<int64_t>());

  const int64_t fps = doc["fps"].as<int64_t>();
  Stats::write(stats::FPS, fps);

  const int64_t echo_now_us = doc["echo_now_µs"].as<int64_t>();
  const auto roundtrip = pet::now_realtime() - pet::from_val<Nanos, Micros>(echo_now_us);
  Stats::write(stats::REMOTE_ROUNDTRIP, roundtrip);
}

void DmxCtrl::listen() noexcept {
  static constexpr csv cat{"data_sock"};
  // listen for inbound data connections and get our local port
  // when this async_accept is successful msg_loop() is invoked
  data_sock.emplace(io_ctx);
  acceptor.async_accept( //
      *data_sock,        //
      [this, e = Elapsed()](const error_code ec) mutable {
        Stats::write(stats::DATA_CONNECT_ELAPSED, e.freeze());

        if (!ec && data_sock.has_value() && data_sock->is_open()) {

          const auto &r = data_sock->remote_endpoint();
          INFO(module_id, cat, "{}\n", io::log_socket_msg(ec, *data_sock, r, e));

          if (!ec) {
            // success, set sock opts, connected and start msg_loop()
            data_sock->set_option(ip_tcp::no_delay(true));

            connected = ctrl_sock->is_open() && data_sock->is_open();

            msg_loop(); // start the msg loop
          }

        } else {
          INFO(module_id, cat, "ctrl_socket={} data_socket={} {}\n", //
               ctrl_sock.has_value(), data_sock.has_value(), ec.message());
        }
      });
}

void DmxCtrl::msg_loop() noexcept {
  // only attempt to read a message if we're connected
  if (connected.load() && ready()) {

    desk::async_read_msg( //
        *ctrl_sock, [this, e = Elapsed()](const error_code ec, desk::Msg msg) mutable {
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

void DmxCtrl::send_ctrl_msg(desk::Msg msg) noexcept {
  static constexpr csv fn_id{"send_ctrl_msg"};

  msg.serialize();
  const auto buf_seq = msg.buff_seq(); // calculates tx_len
  const auto tx_len = msg.tx_len;

  asio::async_write(     //
      ctrl_sock.value(), //
      buf_seq,           //
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
  if (connected.load()) { // only send msgs when connected

    msg.finalize();

    desk::async_write_msg( //
        *data_sock,        //
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

void DmxCtrl::stalled_watchdog() noexcept {
  static constexpr csv cat{"stalled"};

  stalled_timer.expires_after(stalled_timeout());
  stalled_timer.async_wait([this](error_code ec) {
    if (ec == errc::success) {

      const auto was_connected = connected.load();
      if (was_connected) INFO(module_id, cat, "was_connected={}\n", was_connected);

      acceptor.cancel(ec);
      if (ctrl_sock.has_value()) ctrl_sock->cancel(ec);
      if (data_sock.has_value()) data_sock->cancel(ec);
      connected = false;

      listen();
      connect();

      stalled_watchdog(); // restart stalled watchdog

    } else if (ec != errc::operation_canceled) {
      INFO(module_id, cat, "falling through {}\n", ec.message());
    }
  });
}

} // namespace pierre
