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
#include "base/uint8v.hpp"
#include "config/config.hpp"
#include "io/async_msg.hpp"
#include "io/msg.hpp"
#include "mdns/mdns.hpp"

#include <future>
#include <latch>
#include <math.h>
#include <ranges>
#include <utility>

namespace pierre {
namespace desk {

// general API
void Control::connect(const error_code ec) {
  __LOG0(LCOL01 " reason={}\n", moduleID(), "CONNECT", ec.message());

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

  auto buff = std::make_unique<io::Packed>();
  auto doc = std::make_unique<io::StaticDoc>();

  auto &bref = *buff;
  auto &dref = *doc;

  io::async_tld(*socket, bref, dref,
                [=, this, buff = std::move(buff), doc = std::move(doc)](error_code ec,
                                                                        size_t bytes) mutable {
                  // NOTE:  we reuse ec below
                  if (!ec && bytes) [[likely]] {
                    JsonObject root = doc->as<JsonObject>();

                    if (HANDSHAKE == csv(root["type"])) {
                      auto remote_now = Micros(root["now_µs"].as<int64_t>());
                      remote_time_skew = pet::abs(remote_now - pet::now_epoch<Micros>());
                      remote_ref_time = Micros(root["ref_µs"].as<int64_t>());

                      __LOG0(LCOL01 " clock diff={:0.3}\n", moduleID(), "REMOTE",
                             pet::as_millis_fp(remote_time_skew));
                      handshake_reply(port);
                    }
                  }

                  doc.reset();
                  buff.reset();

                  reset_if_needed(ec);
                });
}

void Control::handshake_reply(Port port) {
  auto ctrl_msg = std::make_unique<CtrlMsg>(HANDSHAKE);

  ctrl_msg->add_kv("idle_shutdown_ms", 30000);
  ctrl_msg->add_kv("lead_tims_us", lead_time.count());
  ctrl_msg->add_kv("ref_µs", pet::reference<Micros>().count());
  ctrl_msg->add_kv("data_port", port);

  auto buff_seq = ctrl_msg->buff_seq();

  socket->async_write_some( //
      buff_seq,             // must get buff_seq do to move of unique_ptr
      [this, cm = std::move(ctrl_msg)](const error_code ec, size_t bytes) mutable {
        cm->log_tx(ec, bytes);

        store_ec_last(ec);

        cm.reset();
      });

  msg_loop();
}

void Control::msg_loop() {

  auto work_buff = std::make_shared<io::Packed>();
  auto rx_doc = std::make_shared<io::StaticDoc>();

  io::async_tld(*socket, *work_buff, *rx_doc,
                [=, this](const error_code ec, size_t bytes) mutable {
                  if (!ec && bytes) [[likely]] {
                    JsonObjectConst root = rx_doc->as<JsonObjectConst>();

                    csv type = root["type"];
                    if (type == FEEDBACK) {
                      Micros async_loop(root["async_µs"]);
                      Micros remote_elapsed(root["elapsed_µs"]);
                      Micros echoed_now(root["echoed_now_µs"].as<int64_t>());
                      float fps = root["fps"];

                      auto roundtrip = pet::reference() - echoed_now;

                      // if (elapsed > pet::percent<Nanos>(lead_time, 0.25)) {
                      //   __LOG0(LCOL01 " rt={:0.2}\n", moduleID(), "REMOTE",
                      //   pet::as<MillisFP>(elapsed));
                      // }

                      if ((async_loop > lead_time)) {
                        __LOG0(LCOL01 " async_loop={:0.1} elapsed={:>6} fps={:02.1f}\n",
                               moduleID(), "REMOTE",
                               pet::as_millis_fp(async_loop), //
                               remote_elapsed,                //
                               fps                            //
                        );
                      }
                    }

                    msg_loop();
                  }

                  rx_doc.reset();
                  work_buff.reset();
                });
}

// misc debug
void Control::log_connected(Elapsed &elapsed) {
  __LOG0(LCOL01 " {}:{} -> {}:{} elapsed={:0.2} handle={}\n", moduleID(), "CONNECT",
         socket->local_endpoint().address().to_string(), socket->local_endpoint().port(),
         remote_endpoint->address().to_string(), remote_endpoint->port(), elapsed.as<MillisFP>(),
         socket->native_handle());
}

} // namespace desk
} // namespace pierre
