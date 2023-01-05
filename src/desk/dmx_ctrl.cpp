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
#include "base/uint8v.hpp"
#include "io/async_msg.hpp"
#include "io/msg.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"
#include "lcs/stats.hpp"
#include "mdns/mdns.hpp"

#include <array>
#include <iterator>
#include <latch>
#include <ranges>
#include <utility>

namespace pierre {

static const auto controller_name() noexcept {
  static constexpr csv PATH{"dmx.controller"};

  return config()->at(PATH).value_or<string>("dmx");
}

static const auto idle_shutdown() noexcept {
  static constexpr csv PATH{"dmx.timeouts.milliseconds.idle"};

  return config()->at(PATH).value_or(30000);
}

static const auto stalled_timeout() noexcept {
  static constexpr csv PATH{"dmx.timeouts.milliseconds.stalled"};

  return pet::from_val<Nanos, Millis>(config()->at(PATH).value_or(2000));
}

// general API
DmxCtrl::DmxCtrl(io_context &io_ctx) noexcept
    : io_ctx(io_ctx),       // use shared io_ctx
      local_strand(io_ctx), // serialize
      acceptor(io_ctx, tcp_endpoint{ip_tcp::v4(), ANY_PORT}),
      stalled_timer(io_ctx, Nanos::zero()) // stalled_watchdog() will start on first call
{}

void DmxCtrl::connect() noexcept {
  static constexpr csv cat{"ctrl_sock"};

  // now begin the control channel connect and handshake
  asio::post(io_ctx, // execute on any io_ctx, zero conf resolution may block
             [s = ptr(),
              zcsf = mDNS::zservice(controller_name()), // zerconf resolve future
              e = Elapsed()]() mutable {
               auto zcs = zcsf.get(); // can BLOCK, as needed

               auto &ctrl_sock = s->ctrl_sock.emplace(s->io_ctx); // create the socket
               const auto addr = asio::ip::make_address_v4(zcs.address());

               const std::array endpoints{tcp_endpoint{addr, zcs.port()}};
               asio::async_connect( //
                   ctrl_sock,       //
                   endpoints,       //
                   [s = s->ptr(), e](const error_code ec, const tcp_endpoint r) mutable {
                     if (s->ctrl_sock.has_value()) {

                       auto &ctrl_sock = s->ctrl_sock.value();

                       INFO(module_id, cat, "{}\n", io::log_socket_msg(ec, ctrl_sock, r, e));

                       if (!ec) {

                         ctrl_sock.set_option(ip_tcp::no_delay(true));
                         Stats::write(stats::CTRL_CONNECT_ELAPSED, e.freeze());

                         io::Msg msg(HANDSHAKE);

                         msg.add_kv("idle_shutdown_ms", idle_shutdown());
                         msg.add_kv("lead_time_µs", InputInfo::lead_time);
                         msg.add_kv("ref_µs", pet::now_realtime<Micros>());
                         msg.add_kv("data_port", s->acceptor.local_endpoint().port());

                         s->send_ctrl_msg(std::move(msg));
                       }

                     } else {
                       INFO(module_id, cat, "no socket, {}\n", ec.message());
                     }
                   });
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
      [s = ptr(), e = Elapsed()](const error_code ec) mutable {
        Stats::write(stats::DATA_CONNECT_ELAPSED, e.freeze());

        if (!ec && s->data_sock.has_value() && s->data_sock->is_open()) {
          auto &data_sock = s->data_sock.value();

          const auto &r = data_sock.remote_endpoint();
          INFO(module_id, cat, "{}\n", io::log_socket_msg(ec, data_sock, r, e));

          if (!ec) {
            // success, set sock opts, connected and start msg_loop()
            data_sock.set_option(ip_tcp::no_delay(true));

            s->connected = s->ctrl_sock->is_open() && data_sock.is_open();

            s->msg_loop(); // start the msg loop
          }

        } else {
          INFO(module_id, cat, "ctrl_socket={} data_socket={} {}\n", //
               s->ctrl_sock.has_value(), s->data_sock.has_value(), ec.message());
        }
      });
}

void DmxCtrl::msg_loop() noexcept {
  // only attempt to read a message if we're connected
  if (connected.load() && ready()) {

    io::async_read_msg( //
        *ctrl_sock, [s = ptr(), e = Elapsed()](const error_code ec, io::Msg msg) mutable {
          Stats::write(stats::CTRL_MSG_READ_ELAPSED, e.freeze());

          if (ec == errc::success) {
            s->stalled_watchdog(); // restart stalled watchdog
            // handle the various message types
            if (msg.key_equal(io::TYPE, FEEDBACK)) s->handle_feedback_msg(msg.doc);

            s->msg_loop(); // async handle next message

          } else {
            Stats::write(stats::CTRL_MSG_READ_ERROR, true);
          }
        });
  }
}

void DmxCtrl::send_ctrl_msg(io::Msg msg) noexcept {
  msg.serialize();
  const auto buf_seq = msg.buff_seq(); // calculates tx_len
  const auto tx_len = msg.tx_len;

  asio::async_write(     //
      ctrl_sock.value(), //
      buf_seq,           //
      asio::transfer_exactly(tx_len),
      [s = ptr(), msg = std::move(msg), e = Elapsed()](const error_code ec,
                                                       const auto actual) mutable {
        Stats::write(stats::CTRL_MSG_WRITE_ELAPSED, e.freeze());

        if ((ec != errc::success) && (actual != msg.tx_len)) {
          Stats::write(stats::CTRL_MSG_WRITE_ERROR, true);

          INFO(module_id, "SEND_CTRL_MSG", "write failed, reason={} bytes={}/{}\n", //
               ec.message(), actual, msg.tx_len);
        }
      });
}

void DmxCtrl::send_data_msg(DmxDataMsg msg) noexcept {
  if (connected.load()) { // only send msgs when connected

    msg.finalize();

    io::async_write_msg( //
        *data_sock,      //
        std::move(msg), [s = ptr(), e = Elapsed()](const error_code ec) mutable {
          // better readability

          Stats::write(stats::DATA_MSG_WRITE_ELAPSED, e.freeze());

          if (ec != errc::success) {
            Stats::write(stats::DATA_MSG_WRITE_ERROR, true);
            s->connected.store(false);
          }

          return ec;
        });
  }
}

void DmxCtrl::stalled_watchdog() noexcept {
  stalled_timer.expires_after(stalled_timeout());
  stalled_timer.async_wait( //
      asio::bind_executor(  //
          local_strand,     // sync with connect and listen
          [s = ptr(), was_connected = connected.load()](error_code ec) {
            if (ec == errc::success) {

              if (was_connected) INFO(module_id, "STALLED", "was_connected={}\n", was_connected);

              s->acceptor.cancel(ec);
              if (s->ctrl_sock.has_value()) s->ctrl_sock->cancel(ec);
              if (s->data_sock.has_value()) s->data_sock->cancel(ec);
              s->connected = false;

              s->listen();
              s->connect();

              s->stalled_watchdog(); // restart stalled watchdog

            } else if (ec != errc::operation_canceled) {
              INFO(module_id, "STALLED", "falling through {}\n", ec.message());
            }
          }));
}

} // namespace pierre
