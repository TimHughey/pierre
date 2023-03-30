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
#include "base/input_info.hpp"
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
#include <boost/asio.hpp>
#include <iterator>
#include <latch>
#include <utility>

namespace pierre {
namespace desk {

inline const string get_cfg_host() noexcept {
  return config_val<DmxCtrl, string>("remote.host"sv, "dmx");
}

inline auto resolve_timeout() noexcept {
  return config_val<DmxCtrl, Millis>("local.resolve.timeout.ms"sv, 15'000);
}

inline auto resolve_retry_wait() noexcept {
  return config_val<DmxCtrl, Millis>("local.resolve.retry.ms"sv, 60'000);
}

inline auto cfg_stall_timeout() noexcept {
  return config_val<DmxCtrl, Millis>("local.stalled.ms"sv, 7500);
}

// general API
DmxCtrl::DmxCtrl() noexcept
    : sess_sock(io_ctx),                                          //
      data_accep(io_ctx, data_lep),                               //
      data_sock(io_ctx),                                          // unopened
      stall_timeout{cfg_stall_timeout()},                         //
      stalled_timer(io_ctx, stall_timeout),                       //
      resolve_retry_timer(io_ctx, resolve_retry_wait()),          //
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
  //       they are handled as needed during handshake()

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

  MsgOut msg(HANDSHAKE);

  msg.add_kv(desk::DATA_PORT, data_accep.local_endpoint().port());
  msg.add_kv(desk::FRAME_LEN, DataMsg::frame_len);
  msg.add_kv(desk::FRAME_US, InputInfo::lead_time_us);
  msg.add_kv(desk::IDLE_MS, config_val<DmxCtrl, int64_t>(idle_ms_path, 10000));
  msg.add_kv(desk::STATS_MS, config_val<DmxCtrl, int64_t>(stats_ms_path, 2000));

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
        DynamicJsonDocument doc(desk::Msg::default_doc_size);

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

              const auto data_wait = Micros(doc[desk::DATA_WAIT_US] | 0);
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

void DmxCtrl::resolve_host() noexcept {
  static constexpr csv fn_id{"host.resolve"};

  asio::post(io_ctx, [this]() {
    // get the host we will connect to unless we already have it
    if (cfg_host.empty()) cfg_host = get_cfg_host();

    INFO_AUTO("attempting to resolve '{}'\n", cfg_host);

    // start name resolution via mdns
    auto zcsf = mDNS::zservice(cfg_host);
    auto zcs_status = zcsf.wait_for(resolve_timeout());

    if (zcs_status != std::future_status::ready) {
      INFO_AUTO("resolve timeout for '{}' ({})\n", cfg_host, resolve_timeout());
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

    asio::post(io_ctx, [this, ep = host_endpoint]() {
      auto resolver = std::make_unique<ip_tcp::resolver>(io_ctx);

      resolver->async_resolve(*ep, //
                              [this, resolver = std::move(resolver)](
                                  error_code ec, ip_tcp::resolver::results_type results) {
                                if (ec) {
                                  INFO_AUTO("resolve err={}\n", ec.message());
                                } else {

                                  std::for_each(results.begin(), results.end(), [](const auto &r) {
                                    INFO_AUTO("hostname: {}\n", r.host_name());
                                  });
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

          INFO_AUTO("{}\n", io::log_socket_msg(ec, sess_sock, r, e));

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

  if (ConfigWatch::has_changed(cfg_fut) && (cfg_host != get_cfg_host())) {
    cfg_host.clear();                      // triggers pull from config on next name resolution
    cfg_fut = ConfigWatch::want_changes(); // get a fresh future
    wait = 0s;                             // override wait, config has changed
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

void DmxCtrl::stall_watchdog_cancel() noexcept {
  try {
    stalled_timer.cancel();
  } catch (...) {
  }
}

void DmxCtrl::unknown_host() noexcept {
  static constexpr csv fn_id{"host.unknown"};

  const auto resolve_backoff = resolve_retry_wait();
  INFO_AUTO("{} not found, retry in {}...\n", cfg_host, resolve_backoff);

  cfg_host.clear();
  stall_watchdog_cancel(); // prevent stall watchdog from interferring

  resolve_retry_timer.expires_from_now(resolve_backoff);
  resolve_retry_timer.async_wait([this](error_code ec) {
    if (!ec) {
      INFO_AUTO("retrying host resolution...\n");

      resolve_host();
    }
  });
}

} // namespace desk
} // namespace pierre
