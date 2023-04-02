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
#include "io/log_socket.hpp"
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
    : thread_pool(thread_count),   //
      sock(thread_pool),           //
      sessions(sessions),          //
      master_clock(master_clock),  //
      desk(desk),                  //
      aes(),                       //
      feedback_timer(thread_pool), //
      teardown_in_progress(false)  //
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

void Ctx::feedback_msg() noexcept {}

void Ctx::force_close() noexcept {
  static constexpr csv fn_id("force_close");
  teardown_now = true;

  INFO_AUTO("requested, teardown_now={}\n", teardown_now);

  teardown();

  INFO_AUTO("completed\n");
}

void Ctx::msg_loop(std::shared_ptr<Ctx> self) noexcept {

  if (teardown_now == true) {
    teardown();
    return;
  };

  request.emplace();
  reply.emplace();

  msg_loop_read(self);
}

void Ctx::msg_loop_read(std::shared_ptr<Ctx> self) noexcept {
  net::async_read_msg(sock, *request, aes, [this, self](error_code ec) mutable {
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

      msg_loop_write(self); // send the reply
    } else {
      teardown_now = true;
    }
  });
}

void Ctx::msg_loop_write(std::shared_ptr<Ctx> self) noexcept {
  // build the reply from the request headers and content
  reply->build(this, request->headers, request->content);

  net::async_write_msg(sock, *reply, aes, [this, self](error_code ec) mutable {
    if (!ec) {
      request->record_elapsed();

      msg_loop(self); // prepare for next message

    } else { // error, teardown
      teardown_now = true;
    }
  });
}

void Ctx::peers(const Peers &peer_list) noexcept { master_clock->peers(peer_list); }

void Ctx::run() noexcept {

  // once we've fired up the thread immediately begin the message processing loop
  // by posting work to the io_ctx
  asio::post(thread_pool, [self = shared_from_this()]() mutable { self->msg_loop(self); });
}

Port Ctx::server_port(ports_t server_type) noexcept {
  Port port{0};

  switch (server_type) {

  case ports_t::AudioPort:
    audio_srv = Audio::start(asio::make_strand(thread_pool), this);
    port = audio_srv->port();
    break;

  case ports_t::ControlPort:
    control_srv = Control::start(asio::make_strand(thread_pool));
    port = control_srv->port();
    break;

  case ports_t::EventPort:
    event_srv = Event::start(asio::make_strand(thread_pool));
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