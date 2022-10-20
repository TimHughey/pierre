/*
    Pierre - Custom Light Show for Wiss Landing
    Copyright (C) 2021  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#include "desk/session/ctrl.hpp"
#include "base/logger.hpp"
#include "base/uint8v.hpp"
#include "config/config.hpp"
#include "io/async_msg.hpp"
#include "io/msg.hpp"
#include "mdns/mdns.hpp"

#include <latch>
#include <ranges>
#include <utility>

namespace pierre {
namespace desk {

// general API
void Control::connect(const error_code ec) {
  INFO(moduleID(), "CONNECT", "reason={}\n", ec.message());

  if (ec == errc::operation_canceled) { // break out when canceled
    return;
  }

  auto name = Config::object("desk")["dmx_service"];
  auto zservice = mDNS::zservice(name);

  if (zservice) [[likely]] {
    // zservice is available, attempt connect
    const auto address = asio::ip::make_address_v4(zservice->address());

    socket.emplace(io_ctx);

    // handler as a variable for improved readability
    auto handler = //
        [this, elapsed = Elapsed()](const error_code ec, const tcp_endpoint endpoint) mutable {
          if (!ec) {
            retry_timer.cancel();
            socket->set_option(ip_tcp::no_delay(true));
            remote_endpoint.emplace(endpoint);

            log_connected(elapsed);
            handshake();
          } else {
            schedule_retry(ec);
          }
        };

    // initiate connection (see handler above)
    const auto endpoint = tcp_endpoint(address, zservice->port());
    asio::async_connect(*socket, std::array{endpoint}, handler);
  } else {
    // zservice not available yet, schedule retry
    schedule_retry(errc::resource_unavailable_try_again);
  }
}

void Control::handshake() {
  auto port = data_session.emplace(io_ctx).accept();

  io::async_read_msg(*socket, [=, this](const error_code ec, io::Msg msg) {
    if (!ec) {
      if (msg.key_equal(io::TYPE, HANDSHAKE)) {
        log_handshake(msg.doc);
        handshake_reply(port);
      }
    }
  });
}

void Control::handshake_reply(Port port) {
  io::Msg ctrl_msg(HANDSHAKE);

  ctrl_msg.add_kv("idle_shutdown_ms", 30000);
  ctrl_msg.add_kv("lead_tims_us", lead_time.count());
  ctrl_msg.add_kv("ref_µs", pet::now_monotonic<Micros>().count());
  ctrl_msg.add_kv("data_port", port);

  io::async_write_msg(*socket, std::move(ctrl_msg),
                      [this](const error_code ec) { store_ec_last(ec); });

  msg_loop();
}

void Control::msg_loop() {
  io::async_read_msg(*socket, [this](const error_code ec, io::Msg msg) {
    if (!ec) {
      if (msg.key_equal(io::TYPE, FEEDBACK)) {
        log_feedback(msg.doc);
      }

      msg_loop();
    }
  });
}

void Control::reset(const error_code ec) {
  INFO(moduleID(), "RESET", "reason={}\n", ec.message());

  if (data_session) {
    data_session->shutdown();
  }

  if (socket) {
    [[maybe_unused]] error_code ec;
    socket->cancel(ec);
    socket->close(ec);
  }

  socket.reset();
  remote_endpoint.reset();
}

// misc debug
void Control::log_connected(Elapsed &elapsed) {
  INFO(moduleID(), "CONNECT", "{}:{} -> {}:{} elapsed={:0.2} handle={}\n",
       socket->local_endpoint().address().to_string(), socket->local_endpoint().port(),
       remote_endpoint->address().to_string(), remote_endpoint->port(), elapsed.as<MillisFP>(),
       socket->native_handle());
}

void Control::log_feedback(JsonDocument &doc) {
  // run_stats(desk::FEEDBACKS);

  run_stats(desk::REMOTE_ASYNC, Micros(doc["async_µs"]));
  run_stats(desk::REMOTE_JITTER, Micros(doc["jitter_µs"]));
  run_stats(desk::REMOTE_ELAPSED, Micros(doc["elapsed_µs"]));

  auto roundtrip = pet::now_realtime<Micros>() - Micros(doc["echoed_now_µs"].as<int64_t>());
  run_stats(desk::REMOTE_LONG_ROUNDTRIP, roundtrip);

  // if (jitter > lead_time) {
  //   INFO(  "seq_num={:<8} jitter={:12} elapsed={:8} fps={:03.1f} rt={:03.1}\n", //
  //          moduleID(), "REMOTE",
  //          doc["seq_num"].as<uint32_t>(), // seq_num of the data msg
  //          pet::as_millis_fp(jitter),     // sync_wait jitter
  //          remote_elapsed,                // elapsed time of the async_loop
  //          doc["fps"].as<float>(),        // frames per second
  //          pet::as_millis_fp(roundtrip)   // roundtrip time
  //
  //   );
  // }
}

void Control::log_handshake(JsonDocument &doc) {
  auto remote_now = Micros(doc["now_µs"].as<int64_t>());
  remote_time_skew = pet::abs(remote_now - pet::now_realtime<Micros>());
  remote_ref_time = Micros(doc["ref_µs"].as<int64_t>());

  INFO(moduleID(), "REMOTE", "clock diff={:0.3}\n", pet::as<MillisFP, Micros>(remote_time_skew));
}

} // namespace desk
} // namespace pierre
