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

#include "desk/session/ctrl.hpp"
#include "base/logger.hpp"
#include "base/uint8v.hpp"
#include "config/config.hpp"
#include "io/async_msg.hpp"
#include "io/msg.hpp"
#include "mdns/mdns.hpp"

#include <array>
#include <iterator>
#include <latch>
#include <ranges>
#include <utility>

namespace pierre {
namespace desk {

// general API
Ctrl::Ctrl(io_context &io_ctx, std::shared_ptr<Stats> run_stats) noexcept
    : io_ctx(io_ctx), // use shared io_ctx
      acceptor(io_ctx, tcp_endpoint{ip_tcp::v4(), ANY_PORT}),
      stalled_timer(io_ctx), // detect stalled controllers
      run_stats(run_stats)   // run stats
{}

Ctrl::~Ctrl() noexcept { INFO(module_id, "DESTRUCT", "ready={}\n", ready()); }

void Ctrl::connect() noexcept {
  connecting = true;
  const auto name = Config() //
                        .at("dmx.controller"sv)
                        .value_or<string_view>(string());

  // now begin the control channel connect and handshake
  asio::post(io_ctx, [s = ptr(),                              //
                      zcsf = mDNS::zservice(std::move(name)), // zerconf resolve future
                      e = Elapsed()]() mutable {
    // the following can BLOCK, as needed
    auto zcs = zcsf.get();

    auto &ctrl_sock = s->ctrl_sock.emplace(s->io_ctx); // create the socket
    const auto addr = asio::ip::make_address_v4(zcs.address());

    const std::array endpoints{tcp_endpoint{addr, zcs.port()}};
    asio::async_connect(
        ctrl_sock, endpoints, [s, e](const error_code ec, const tcp_endpoint r) mutable {
          if (s->log_connect(ec, e, r) == errc::success) [[likely]] {
            s->ctrl_sock->set_option(ip_tcp::no_delay(true));

            io::Msg msg(HANDSHAKE);
            auto idle_ms = pet::from_ms(Config().at("dmx.timeouts.idle_ms"sv).value_or(30000));

            msg.add_kv("idle_shutdown_ms", idle_ms);
            msg.add_kv("lead_time_µs", InputInfo::lead_time);
            msg.add_kv("ref_µs", pet::now_realtime<Micros>());
            msg.add_kv("data_port", s->acceptor.local_endpoint().port());

            s->send_ctrl_msg(std::move(msg));

          } else { // connect error
            [[maybe_unused]] error_code ec;
            s->acceptor.cancel(ec); // this will set the promise
          }
        });
  });

  // NOTE: intentionally fall through
  //       listen() will start msg_loop() when data connection is available
}

std::shared_ptr<Ctrl> Ctrl::init() noexcept {
  auto was_connected = std::atomic_exchange(&connected, false);

  INFO(module_id, "INIT", "was_connected={} connecting={}\n", was_connected, connecting.load());

  // if already connected but not currently connecting
  if (was_connected || connecting.load()) {
    connecting = true;

    const auto retry =
        pet::from_ms(Config().at("dmx.controller.timeouts.retry_ms"sv).value_or(500));
    auto reconnect_timer = std::make_shared<steady_timer>(io_ctx);
    reconnect_timer->expires_after(retry);

    reconnect_timer->async_wait([reconnect_timer, s = ptr()](const error_code ec) {
      if (ec == errc::success) {
        s->listen();
        s->connect();
      }
    });
  } else { // startup case
    connecting = true;

    listen();
    connect();
  }

  return shared_from_this();
}

void Ctrl::listen() noexcept {
  stalled_watchdog();

  // listen for inbound data connections and get our local port
  // when this async_accept is successful msg_loop() is invoked
  data_sock.emplace(io_ctx);
  acceptor.async_accept( //
      *data_sock,        //
      [e = Elapsed(), s = ptr()](const error_code ec) mutable {
        e.freeze(); // stop measuring the accept elapsed time

        // if success, set socket opts, flags then start the message loop
        if (s->log_accept(ec, e) == errc::success) {
          s->data_sock->set_option(ip_tcp::no_delay(true));

          s->connected = s->ctrl_sock->is_open() && s->data_sock->is_open();
          s->connecting = false;

          s->msg_loop(); // start the msg loop
        }
      });
}

void Ctrl::msg_loop() noexcept {
  // only attempt to read a message if we're connected
  if (connected.load()) {
    stalled_watchdog(); // start (or restart) stalled watchdog

    io::async_read_msg(*ctrl_sock, [s = ptr(), e = Elapsed()](const error_code ec, io::Msg msg) {
      if (s->log_read_msg(ec, e) == errc::success) {

        // handle the various message types
        if (msg.key_equal(io::TYPE, FEEDBACK)) s->log_feedback(msg.doc);
        if (msg.key_equal(io::TYPE, HANDSHAKE)) s->log_handshake(msg.doc);

        s->msg_loop(); // async handle next message
      }
    });
  }
}

void Ctrl::send_ctrl_msg(io::Msg msg) noexcept {

  msg.serialize();
  const auto buf_seq = msg.buff_seq(); // calculates tx_len
  const auto tx_len = msg.tx_len;

  asio::async_write( //
      ctrl_sock.value(), buf_seq, asio::transfer_exactly(tx_len),
      [s = ptr(), msg = std::move(msg), e = Elapsed()](const error_code ec,
                                                       const auto actual) mutable {
        s->log_send_ctrl_msg(ec, msg.tx_len, actual, e);
      });
}

void Ctrl::send_data_msg(DataMsg data_msg) noexcept {

  if (connected.load()) {
    io::async_write_msg(
        *data_sock, std::move(data_msg),
        [s = ptr(), e = Elapsed()](const error_code ec) mutable { s->log_send_data_msg(ec, e); });
  }
}

void Ctrl::stalled_watchdog() noexcept {
  if (connected.load()) {
    const auto stalled = pet::from_ms( //
        Config().at("dmx.controller.timeouts.stalled_ms"sv).value_or(0));

    stalled_timer.expires_after(stalled > 0ms ? stalled : 2s);
    stalled_timer.async_wait([s = ptr()](const error_code ec) {
      if (ec == errc::success) {
        s->init();
      }
    });
  }
}

// misc debug
const error_code Ctrl::log_accept(const error_code ec, Elapsed e) noexcept {
  static constexpr auto aborted{io::make_error(errc::operation_canceled)};

  if (ec) {
    if (ec != aborted) {
      // we ignore operation aborted (cancled)
      INFO(module_id, "ACCEPT", "accept failed, reason={}\n", ec.message());
    }
  } else if (!ec) {
    auto r = data_sock->remote_endpoint();
    INFO(module_id, "DATA", "accepted connection {}:{} {}\n", r.address().to_string(), r.port(),
         e.humanize());

    run_stats->write(desk::CTRL_CONNECT_ELAPSED, e);
  }

  return ec;
}

const error_code Ctrl::log_connect(const error_code ec, Elapsed &e,
                                   const tcp_endpoint &r) noexcept {

  string msg;
  auto w = std::back_inserter(msg);

  if (ec) {
    fmt::format_to(w, "failed, reason={}", ec.message());
  } else if (ctrl_sock.has_value() == false) {
    fmt::format_to(w, "ctrl_sock=EMPTY");
  } else {
    const auto &l = ctrl_sock->local_endpoint();

    fmt::format_to(w, "{}:{}", l.address().to_string(), l.port());
    fmt::format_to(w, " -> ");
    fmt::format_to(w, "{}:{}", r.address().to_string(), r.port());
    fmt::format_to(w, " elapsed={} handle={}", e.humanize(), ctrl_sock->native_handle());
  }

  if (!msg.empty()) INFO(module_id, "CONNECT", "{}\n", msg);

  return ec;
}

void Ctrl::log_feedback(JsonDocument &doc) noexcept {
  const auto remote_now = Micros(doc["now_µs"].as<int64_t>());
  const auto clocks_diff = pet::abs(remote_now - pet::now_realtime<Micros>());
  run_stats->write(desk::CLOCKS_DIFF, clocks_diff);

  run_stats->write(desk::REMOTE_DATA_WAIT, Micros(doc["data_wait_µs"] | 0));
  run_stats->write(desk::REMOTE_ELAPSED, Micros(doc["elapsed_µs"] | 0));
  run_stats->write(desk::FPS, doc["fps"].as<int64_t>() | 0);

  const auto roundtrip = pet::now_realtime() - pet::from_us(doc["echo_now_µs"]);
  run_stats->write(desk::REMOTE_ROUNDTRIP, roundtrip);
}

void Ctrl::log_handshake(JsonDocument &doc) {
  const auto remote_now = Micros(doc["now_µs"].as<int64_t>());
  const auto clocks_diff = pet::abs(remote_now - pet::now_realtime<Micros>());

  run_stats->write(desk::CLOCKS_DIFF, clocks_diff);

  INFO(module_id, "REMOTE", "clocks_diff={} ctrl={} data={}\n", //
       pet::humanize(clocks_diff),                              //
       ctrl_sock.has_value() ? ctrl_sock->native_handle() : 0,
       data_sock.has_value() ? data_sock->native_handle() : 0);
}

const error_code Ctrl::log_read_msg(const error_code ec, Elapsed e) noexcept {
  run_stats->write(desk::CTRL_MSG_READ_ELAPSED, e);

  if (ec != errc::success) run_stats->write(desk::CTRL_MSG_READ_ERROR, true);

  return ec;
}

const error_code Ctrl::log_send_ctrl_msg(const error_code ec, const size_t tx_want,
                                         const size_t tx_actual, Elapsed e) noexcept {
  run_stats->write(desk::CTRL_MSG_WRITE_ELAPSED, e);

  if ((ec != errc::success) && (tx_want != tx_actual)) {
    run_stats->write(desk::CTRL_MSG_WRITE_ERROR, true);

    INFO(module_id, "SEND_CTRL_MSG", "write failed, reason={} bytes={}/{}\n", //
         ec.message(), tx_actual, tx_want);
  }

  return ec;
}

const error_code Ctrl::log_send_data_msg(const error_code ec, Elapsed e) noexcept {
  run_stats->write(desk::DATA_MSG_WRITE_ELAPSED, e);
  run_stats->write(desk::FRAMES, 1);

  if (ec != errc::success) run_stats->write(desk::DATA_MSG_WRITE_ERROR, true);

  return ec;
}

} // namespace desk
} // namespace pierre
