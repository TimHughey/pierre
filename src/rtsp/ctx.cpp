//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2022  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#include "ctx.hpp"
#include "audio.hpp"
#include "base/thread_util.hpp"
#include "control.hpp"
#include "event.hpp"
#include "frame/anchor.hpp"
#include "frame/master_clock.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"
#include "lcs/stats.hpp"
#include "mdns/mdns.hpp"
#include "net.hpp"
#include "rtsp/aes.hpp"
#include "rtsp/headers.hpp"
#include "rtsp/reply.hpp"
#include "rtsp/request.hpp"
#include "saver.hpp"

#include <exception>

namespace pierre {
namespace rtsp {

Ctx::Ctx(tcp_socket &&peer, Sessions *sessions, MasterClock *master_clock, Desk *desk) noexcept
    : sock(io_ctx),                                    //
      sessions(sessions),                              //
      master_clock(master_clock),                      //
      desk(desk),                                      //
      aes(),                                           //
      feedback_timer(io_ctx),                          //
      shutdown_latch(std::make_shared<std::latch>(1)), //
      teardown_in_progress(false)                      //
{
  static constexpr csv fn_id{"construct"};
  INFO_INIT("sizeof={:>5} socket={}\n", sizeof(Ctx), peer.native_handle());

  // transfer the accepted connection to our io_ctx
  error_code ec;
  sock.assign(ip_tcp::v4(), peer.release(), ec);

  const auto &r = sock.remote_endpoint();
  const auto msg = io::log_socket_msg(ec, sock, r);
  INFO_AUTO("{}\n", msg);
}

Ctx::~Ctx() noexcept {

  try {
    if (sock.is_open()) {
      sock.shutdown(tcp_socket::shutdown_both);
      sock.close();
    }
  } catch (...) {
  }

  if (!io_ctx.stopped()) io_ctx.stop();

  if (shutdown_latch) shutdown_latch->wait();
}

void Ctx::feedback_msg() noexcept {}

void Ctx::force_close() noexcept {
  static constexpr csv fn_id("force_close");
  teardown_now = true;

  INFO_AUTO("requested, teardown_now={}\n", teardown_now);

  teardown();

  INFO_AUTO("completed\n");
}

void Ctx::msg_loop() noexcept {

  if (teardown_now == true) {
    teardown();
    return;
  };

  request.emplace();
  reply.emplace();

  msg_loop_read();
}

void Ctx::msg_loop_read() noexcept {
  net::async_read_msg(sock, *request, aes, [this](error_code ec) mutable {
    if (!ec) {

      // apply message header to ctx
      const Headers &h = request->headers;

      cseq = h.val<int64_t>(hdr_type::CSeq);
      active_remote = h.val<int64_t>(hdr_type::DacpActiveRemote);
      if (dacp_id.empty()) dacp_id = h.val<string>(hdr_type::DacpID);

      if (h.contains(hdr_type::XAppleProtocolVersion))
        proto_ver = h.val<int64_t>(hdr_type::XAppleProtocolVersion);

      if (h.contains(hdr_type::XAppleClientName) && client_name.empty())
        client_name = h.val(hdr_type::XAppleClientName);

      if (user_agent.empty()) user_agent = h.val(hdr_type::UserAgent);

      msg_loop_write(); // send the reply
    } else {
      teardown_now = true;
    }
  });
}

void Ctx::msg_loop_write() noexcept {
  // build the reply from the request headers and content
  reply->build(this, request->headers, request->content);

  net::async_write_msg(sock, *reply, aes, [this](error_code ec) mutable {
    if (!ec) {
      request->record_elapsed();

      msg_loop(); // prepare for next message

    } else { // error, teardown
      teardown_now = true;
    }
  });
}

void Ctx::peers(const Peers &peer_list) noexcept { master_clock->peers(peer_list); }

void Ctx::run() noexcept {

  // once we've fired up the thread immediately begin the message processing loop
  // by posting work to the io_ctx
  asio::post(io_ctx, std::bind(&Ctx::msg_loop, this));

  // pass in a shared pointer to our self so we stay in scope until
  // io_ctx is out of work (thread exits)
  std::jthread([this, s = shared_from_this(), latch = shutdown_latch]() mutable {
    const auto thread_name = thread_util::set_name(Ctx::thread_name);
    INFO_THREAD_START();

    io_ctx.run();
    latch->count_down();
    latch.reset();
    sessions->erase(s);

    INFO_THREAD_STOP();
  }).detach();
}

Port Ctx::server_port(ports_t server_type) noexcept {
  Port port{0};

  switch (server_type) {

  case ports_t::AudioPort:
    audio_srv = Audio::start(io_ctx, this);
    port = audio_srv->port();
    break;

  case ports_t::ControlPort:
    control_srv = Control::start(io_ctx);
    port = control_srv->port();
    break;

  case ports_t::EventPort:
    event_srv = Event::start(io_ctx);
    port = event_srv->port();
    break;
  }

  return port;
}

void Ctx::set_live() noexcept { sessions->live(this); }

void Ctx::teardown() noexcept {
  static constexpr csv fn_id{"teardown"};

  // only start the teardown if not already in progress
  if (teardown_in_progress.exchange(true) == false) {

    INFO_AUTO("requested {}\n", *this);

    try {
      sock.close();
    } catch (const std::exception &e) {
      INFO_AUTO("failed to shutdown, close socket reason={}\n", e.what());
    }

    group_contains_group_leader = false;

    [[maybe_unused]] error_code ec;
    feedback_timer.cancel(ec);

    if (audio_srv) {
      audio_srv->teardown();
      audio_srv.reset();
    }

    if (control_srv) {
      control_srv->teardown();
      control_srv.reset();
    }

    if (event_srv) {
      event_srv->teardown();
      event_srv.reset();
    }

    INFO_AUTO("completed {}\n", *this);
  }
}

} // namespace rtsp
} // namespace pierre