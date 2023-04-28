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
#include "base/conf/token.hpp"
#include "base/input_info.hpp"
#include "base/logger.hpp"
#include "base/pet.hpp"
#include "base/stats.hpp"
#include "desk/async/read.hpp"
#include "desk/async/write.hpp"
#include "desk/msg/data.hpp"
#include "desk/msg/in.hpp"
#include "desk/msg/out.hpp"
#include "mdns/mdns.hpp"

#include <array>
#include <boost/asio/consign.hpp>
#include <iterator>
#include <utility>

namespace pierre {
namespace desk {

static const string log_socket_msg(error_code ec, tcp_socket &sock, const tcp_endpoint &r,
                                   Elapsed e = Elapsed()) noexcept;

inline const string cfg_host(conf::token &tok) noexcept {
  return tok.val<string>("remote.host"_tpath, "dmx"sv);
}

inline auto resolve_timeout(conf::token &tok) noexcept {
  return tok.val<Millis>("local.resolve.timeout.ms"_tpath, 15'000);
}

inline auto resolve_retry_wait(conf::token &tok) noexcept {
  return tok.val<Millis>("local.resolve.retry.ms"_tpath, 60'000);
}

inline auto cfg_stall_timeout(conf::token &tok) noexcept {
  return tok.val<Millis>("local.stalled.ms"_tpath, 7500);
}

// general API
DmxCtrl::DmxCtrl(asio::io_context &io_ctx) noexcept
    : tokc(module_id),
      // creqte strand for serialized session comms
      sess_strand(asio::make_strand(io_ctx)),
      // create a strand for serialized data comms
      data_strand(asio::make_strand(io_ctx)),
      // create the session socket
      sess_sock(sess_strand),
      // create the data connection acceptor
      data_accep(data_strand, data_lep),
      // create the (unopened) data socket connection
      data_sock(data_strand),
      // create the stall timer, use data strand
      stalled_timer(data_strand, 7500ms),
      // create the resolve timeout timer, use sess strand
      resolve_retry_timer(sess_strand, 1s) //
{
  // now that tokcen is populated we can assign the remaining config vars
  // capture stall timeout config
  stall_timeout = cfg_stall_timeout(tokc);

  // here we post resolve_host() to the io_context we're about to start
  //
  // resolve_host() may BLOCK or FAIL
  //  -if successful, handshake() is called to setup the control and data sockets
  //  -if failure, unknown_host() is invoked to retry resolution
  resolve_host();

  // NOTE: we do not start listening or the stalled timer at this point
  //       they are handled as needed during handshake()

  INFO_INIT("sizeof={:>5} thread_count={} {}\n", sizeof(DmxCtrl), required_threads(),
            tokc.msg(conf::token::Info));
}

void DmxCtrl::handshake() noexcept {
  static constexpr csv fn_id{"handshake"};
  static constexpr csv idle_ms_path{"remote.idle_shutdown.ms"};
  static constexpr csv stats_ms_path{"remote.stats.ms"};

  MsgOut msg(HANDSHAKE);

  msg.add_kv(desk::DATA_PORT, data_accep.local_endpoint().port());
  msg.add_kv(desk::FRAME_LEN, DataMsg::frame_len);
  msg.add_kv(desk::FRAME_US, InputInfo::lead_time_us);
  msg.add_kv(desk::IDLE_MS, tokc.val<int64_t>(idle_ms_path, 10000));
  msg.add_kv(desk::STATS_MS, tokc.val<int64_t>(stats_ms_path, 2000));

  async::write_msg(sess_sock, std::move(msg), [this](MsgOut msg) {
    if (msg.elapsed() > 750us) Stats::write(stats::DATA_MSG_WRITE_ELAPSED, msg.elapsed());

    if (msg.xfer_error()) {
      Stats::write(stats::DATA_MSG_WRITE_ERROR, true);

      INFO_AUTO("write failed, err={}\n", msg.ec.message());
    }
  });
}

void DmxCtrl::msg_loop(MsgIn &&msg) noexcept {
  static constexpr csv fn_id{"msg_loop"};

  // only attempt to read a message if we're connected
  if (connected) {

    async::read_msg(sess_sock, std::forward<MsgIn>(msg), [this](MsgIn &&msg) {
      if (msg.elapsed() > 30ms) Stats::write(stats::DATA_MSG_READ_ELAPSED, msg.elapsed());

      if (msg.xfer_ok()) {
        StaticJsonDocument<desk::Msg::default_doc_size> doc;

        if (msg.deserialize_into(doc)) {

          // handle the various message types
          if (Msg::is_msg_type(doc, desk::STATS)) {
            const auto echo_now_us = doc[desk::ECHO_NOW_US].as<int64_t>();
            const auto remote_roundtrip = pet::elapsed_from_raw<Micros>(echo_now_us);

            Stats::write(stats::REMOTE_ROUNDTRIP, remote_roundtrip);

            if (doc[desk::SUPP] | false) {

              // std::array<char, 256> pbuf{0x00};
              // serializeJsonPretty(doc, pbuf.data(), pbuf.size());
              // INFO_AUTO("\n{}\n", pbuf.data());

              const auto data_wait = Micros(doc[desk::DATA_WAIT_US] | 0) - InputInfo::lead_time;
              Stats::write(stats::REMOTE_DATA_WAIT, data_wait);

              Stats::write(stats::REMOTE_DMX_QOK, doc[desk::QOK].as<int64_t>());
              Stats::write(stats::REMOTE_DMX_QRF, doc[desk::QRF].as<int64_t>());
              Stats::write(stats::REMOTE_DMX_QSF, doc[desk::QSF].as<int64_t>());

              // ruth sends FPS as an int * 100, convert to double
              const double fps = doc[desk::FPS].as<int64_t>() / 100.0;
              Stats::write(stats::FPS, fps);
            }
          }
        }

        // we are finished with the message, safe to reuse
        msg_loop(std::move(msg));

      } else {
        Stats::write(stats::DATA_MSG_READ_ERROR, true);
        connected = false;
      }
    });
  }
}

int DmxCtrl::required_threads() noexcept { return tokc.val<int>("threads", 2); }

void DmxCtrl::resolve_host() noexcept {
  static constexpr auto fn_id{"host.resolve"sv};

  asio::post(sess_strand, [this]() {
    // get the host we will connect to unless we already have it
    if (remote_host.empty()) remote_host = cfg_host(tokc);

    INFO_AUTO("attempting to resolve '{}'\n", remote_host);

    // start name resolution via mdns
    auto zcsf = mDNS::zservice(remote_host);
    auto zcs_status = zcsf.wait_for(resolve_timeout(tokc));

    if (zcs_status != std::future_status::ready) {
      INFO_AUTO("resolve timeout for '{}' ({})\n", remote_host, resolve_timeout(tokc));
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
    stall_watchdog();

    // we'll connect to this endpoint
    const auto remote_ip_addr = asio::ip::make_address_v4(zcs.address());
    host_endpoint.emplace(remote_ip_addr, zcs.port());

    asio::post(sess_strand, [this, ep = host_endpoint]() {
      using resolver_type = ip_tcp::resolver;
      using resolver_results = ip_tcp::resolver::results_type;

      auto resolver = std::make_unique<resolver_type>(sess_strand);

      resolver->async_resolve(*ep, [this, resolver = std::move(resolver)](
                                       const error_code &ec, resolver_results r) mutable {
        if (!ec) {
          std::for_each(r.begin(), r.end(),
                        [](const auto &r) { INFO_AUTO("host={}\n", r.host_name()); });

          if (ec.message() != "Success") INFO_AUTO("resolve err={}\n", ec.message());
        }
      });
    });

    // ensure the data socket is closed, we're about to accept a new connection
    if (data_connected.exchange(false) == true) {
      [[maybe_unused]] error_code ec;
      data_sock.close(ec);
    }

    // listen for the inbound data socket connection from ruth
    // (response to handshake message)
    data_accep.async_accept(data_sock, [this](const error_code &ec) {
      if (!ec) {
        data_sock.set_option(ip_tcp::no_delay(true));
        data_connected.exchange(true);
      }
    });

    // kick-off an async connect. once connected we'll send the handshake
    // that contains the data port for the remote host to connect
    asio::async_connect( //
        sess_sock, std::array{host_endpoint.value()},
        [this, e = Elapsed()](const error_code ec, const tcp_endpoint r) mutable {
          static constexpr csv fn_id{"host.connect"};

          INFO_AUTO("{}\n", log_socket_msg(ec, sess_sock, r, e));

          if (!ec) {
            sess_sock.set_option(ip_tcp::no_delay(true));
            Stats::write(stats::DATA_CONNECT_ELAPSED, e.freeze());

            connected = true;

            msg_loop(MsgIn());
            handshake();
          }
        });
  });
}

void DmxCtrl::send_data_msg(DataMsg &&data_msg) noexcept {
  if (data_connected) { // only send msgs when connected

    async::write_msg(data_sock, std::move(data_msg), [this](DataMsg msg) {
      if (msg.elapsed() > 200us) Stats::write(stats::DATA_MSG_WRITE_ELAPSED, msg.elapsed());

      if (msg.xfer_ok()) {
        stall_watchdog();
      } else {
        Stats::write(stats::DATA_MSG_WRITE_ERROR, true);
        connected.store(false);
      }
    });
  }
}

void DmxCtrl::stall_watchdog(Millis wait) noexcept {
  static constexpr csv fn_id{"stalled"};

  // when passed a non-zero wait time we're handling a special
  // case (e.g. host resolve failure), otherwise use configured
  // stall timeout value
  if (wait == Millis::zero()) wait = stall_timeout;

  if (remote_host != cfg_host(tokc)) {
    remote_host.clear(); // triggers pull from config on next name resolution
    wait = 0s;           // override wait, config has changed
  }

  stalled_timer.expires_after(wait);
  stalled_timer.async_wait([this, wait = wait](error_code ec) {
    if (ec == errc::success) {

      if (const auto was_connected = std::atomic_exchange(&connected, false)) {
        INFO_AUTO("was_connected={} stall_timeout={}\n", was_connected, wait);
      }

      data_sock.close(ec);

      resolve_host(); // kick off name resolution, connect sequence

    } else if (ec != errc::operation_canceled) {
      INFO_AUTO("falling through {}\n", ec.message());
    }
  });
}

void DmxCtrl::unknown_host() noexcept {
  static constexpr csv fn_id{"host.unknown"};

  const auto resolve_backoff = resolve_retry_wait(tokc);
  INFO_AUTO("{} not found, retry in {}...\n", remote_host, resolve_backoff);

  remote_host.clear();

  // setting a timer's expires auto cancel's running timer
  resolve_retry_timer.expires_from_now(resolve_backoff);
  resolve_retry_timer.async_wait([this](error_code ec) {
    if (!ec) {
      INFO_AUTO("retrying host resolution...\n");

      resolve_host();
    }
  });
}

////
//// free function for logging socket info
////

const string log_socket_msg(error_code ec, tcp_socket &sock, const tcp_endpoint &r,
                            Elapsed e) noexcept {
  e.freeze();

  string msg;
  auto w = std::back_inserter(msg);

  auto open = sock.is_open();
  fmt::format_to(w, "{} ", open ? "[OPEN]" : "[CLSD]");

  try {
    if (open) {

      const auto &l = sock.local_endpoint();

      fmt::format_to(w, "{:>15}:{:<5} {:>15}:{:<5}",    //
                     l.address().to_string(), l.port(), //
                     r.address().to_string(), r.port());
    }

    if (ec != errc::success) fmt::format_to(w, " {}", ec.message());
  } catch (const std::exception &e) {

    fmt::format_to(w, "EXCEPTION {}", e.what());
  }

  if (e > 1us) fmt::format_to(w, " {}", e.humanize());

  return msg;
}

} // namespace desk
} // namespace pierre
