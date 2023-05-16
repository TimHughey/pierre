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
#include "base/logger.hpp"
#include "base/stats.hpp"
#include "control.hpp"
#include "desk/desk.hpp"
#include "event.hpp"
#include "frame/anchor.hpp"
#include "mdns/mdns.hpp"
#include "net.hpp"
#include "rtsp/aes.hpp"
#include "rtsp/headers.hpp"
#include "rtsp/reply.hpp"
#include "rtsp/request.hpp"
#include "rtsp/rtsp.hpp"
#include "saver.hpp"

#include <exception>
#include <pthread.h>

namespace pierre {
namespace rtsp {

static const string log_socket_msg(error_code ec, tcp_socket &sock,
                                   const tcp_endpoint &r) noexcept {

  string msg;
  auto w = std::back_inserter(msg);

  auto open = sock.is_open();
  fmt::format_to(w, "{}", open ? "[O]" : "[C]");

  if (open) {
    const auto &l = sock.local_endpoint();

    fmt::format_to(w, " {:20} {:20}", fmt::format("{}:{}", l.address().to_string(), l.port()),
                   fmt::format("{}:{}", r.address().to_string(), r.port()));
  }

  if (ec != errc::success) fmt::format_to(w, " {}", ec.message());

  return msg;
}

Ctx::Ctx(tcp_socket &&peer, Rtsp *rtsp, Desk *desk) noexcept
    : sock(io_ctx), rtsp(rtsp), desk(desk), aes(), feedback_timer(io_ctx),
      teardown_in_progress{false} //
{
  INFO_INIT("sizeof={:>5} socket={}\n", sizeof(Ctx), peer.native_handle());

  if (desk) desk->resume();

  // transfer the accepted connection to our io_ctx
  error_code ec;
  sock.assign(ip_tcp::v4(), peer.release(), ec);

  const auto &r = sock.remote_endpoint();
  const auto msg = log_socket_msg(ec, sock, r);
  INFO_INIT("{}\n", msg);

  thread = std::jthread([this]() {
    constexpr auto tname{"pierre_sess"};

    pthread_setname_np(pthread_self(), tname);

    live = true;

    msg_loop(); // queues the message handling work

    io_ctx.run();
  });
}

Ctx::~Ctx() noexcept {
  if (thread.joinable()) thread.join();
}

void Ctx::feedback_msg() noexcept {}

void Ctx::force_close() noexcept {
  constexpr auto fn_id{"force_close"sv};

  bool is_live{true};
  if (std::exchange(live, is_live) == false) {
    INFO_AUTO("requested, is_live={} live={}\n", is_live, live);

    teardown();

  } else {
    INFO_AUTO("requested however wasn't live\n");
  }
}

void Ctx::msg_loop() noexcept {

  if (live == true) {
    request.emplace();
    reply.emplace();

    msg_loop_read();

  } else {
    teardown();
  }
}

void Ctx::msg_loop_read() noexcept {
  net::async_read_msg(sock, *request, aes, [this](error_code ec) {
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
      force_close();
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
      force_close();
    }
  });
}

Port Ctx::server_port(ports_t server_type) noexcept {
  Port port{0};

  switch (server_type) {

  case ports_t::AudioPort:
    audio_srv = std::make_unique<Audio>(this);
    port = audio_srv->port();
    break;

  case ports_t::ControlPort:
    control_srv = std::make_unique<Control>(io_ctx);
    port = control_srv->port();
    break;

  case ports_t::EventPort:
    event_srv = std::make_unique<Event>(io_ctx);
    port = event_srv->port();
    break;
  }

  return port;
}

void Ctx::set_live() noexcept {
  rtsp->set_live(this);

  desk->resume();
}

void Ctx::teardown() noexcept {
  static constexpr csv fn_id{"teardown"};

  // only start the teardown if not already in progress
  if (std::exchange(teardown_in_progress, true) == false) {

    INFO_AUTO("requested {}\n", *this);

    error_code ec;
    feedback_timer.cancel(ec);
    sock.shutdown(tcp_socket::shutdown_both, ec);
    sock.close(ec);

    if (ec) INFO_AUTO("failed to shutdown / close socket, reason={}\n", ec.what());

    group_contains_group_leader = false;

    audio_srv.reset();
    control_srv.reset();
    event_srv.reset();

    INFO_AUTO("completed {}\n", *this);
  }
}

} // namespace rtsp
} // namespace pierre