/*
    Pierre - Custom Light Show via DMX for Wiss Landing
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

#pragma once

#include "base/elapsed.hpp"
#include "base/io.hpp"
#include "base/pet.hpp"
#include "base/typical.hpp"
#include "base/uint8v.hpp"
#include "desk/session/data.hpp"
#include "desk/stats.hpp"
#include "io/msg.hpp"

#include <array>
#include <memory>
#include <optional>
#include <ranges>

namespace pierre {
namespace desk {

class Control {
public:
  Control(io_context &io_ctx, strand &streams_strand, error_code &ec_last, const Nanos lead_time,
          desk::stats &stats)
      : io_ctx(io_ctx),                                  // use shared io_ctx
        streams_strand(streams_strand),                  // syncronize shared status
        ec_last(ec_last),                                // shared state
        lead_time(lead_time),                            // grab a local copy of lead time
        stats(stats), retry_time(Nanos(lead_time * 44)), // duration to wait for reconnect, zservice available
        retry_timer(io_ctx, retry_time),                 // timer for zservice and connect retry
        state(INITIALIZE)                                // decides which and how ctrl msgs are handled
  {
    connect(); // start connect sequence when constructed
  }

  tcp_socket &data_socket() { return data_session->socket(); }

  bool ready() { return socket && socket->is_open() && data_session && data_session->socket().is_open(); }

  void shutdown(strand &streams_strand) {
    asio::post(streams_strand, [this] { reset(io::make_error(errc::operation_canceled)); });
  }

  // misc debug
  static constexpr csv moduleID() { return module_id; }

private:
  void connect() { connect(std::make_error_code(std::errc::not_connected)); }
  void connect(const error_code ec);

  // wait for handshake message from remote endpoint
  void handshake();
  void handshake_reply(Port port);

  void reset() { reset(io::make_error(errc::connection_reset)); }
  void reset(auto val) { reset(io::make_error(val)); }
  void reset(const error_code ec) {
    __LOG0(LCOL01 " reason={}\n", moduleID(), "RESET", ec.message());

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
  void reset_if_needed(const error_code ec) {
    if (ec) {
      reset(ec);
    }
  }

  void msg_loop();

  void schedule_retry(errc::errc_t errc_val) {
    schedule_retry(error_code(errc_val, sys::generic_category()));
  }

  void schedule_retry(const error_code retry_ec) {
    __LOG0(LCOL01 " reason={}\n", moduleID(), "RETRY", retry_ec.message());

    if (retry_ec != errc::operation_canceled) {
      reset(retry_ec);

      retry_timer.expires_after(retry_time);
      retry_timer.async_wait([=, this](const error_code timer_ec) {
        if (!timer_ec) { // catch operation cancel
          connect(retry_ec);
        }
      });
    }
  }

  void store_ec_last(const error_code ec) {
    asio::defer(streams_strand, [=, this]() { ec_last = ec; });
  }

  // watchdogs

  // misc debug
  void log_connected(Elapsed &elapsed);
  void log_feedback(JsonDocument &doc);
  void log_handshake(JsonDocument &doc);

private:
  // order dependent
  io_context &io_ctx;
  strand &streams_strand;
  error_code &ec_last;
  Nanos lead_time;
  desk::stats &stats;
  const Nanos retry_time;
  steady_timer retry_timer;
  enum { INITIALIZE, RUN, SHUTDOWN } state;

  // order independent
  std::optional<tcp_socket> socket;
  std::optional<tcp_endpoint> remote_endpoint;
  std::optional<Data> data_session;
  std::future<bool> data_connected;

  // remote time keeping
  Micros remote_ref_time;
  Micros remote_time_skew;

  // ctrl message types
  static constexpr csv FEEDBACK{"feedback"};
  static constexpr csv HANDSHAKE{"handshake"};

  // misc debug
  static constexpr csv module_id{"DESK_CONTROL"};
};

} // namespace desk
} // namespace pierre