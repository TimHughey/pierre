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
#include "control.hpp"
#include "event.hpp"
#include "frame/anchor.hpp"
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

void Ctx::close() noexcept {
  static constexpr csv fn_id{"close"};

  if (teardown_in_progress == true) {
    error_code ec;
    sock->shutdown(tcp_socket::shutdown_both, ec);
    sock->close(ec);
    sock.reset();

    INFO(module_id, fn_id, "socket shutdown and closed, {}\n", ec.what());
  }
}

void Ctx::feedback_msg() noexcept {}

void Ctx::msg_loop() noexcept {

  request.emplace();
  reply.emplace();

  msg_loop_read();
}

void Ctx::msg_loop_read() noexcept {

  net::async_read_msg(ptr(), [this, ctx = ptr()](error_code ec) mutable {
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

    } else { // error, teardown
      teardown();
    }
  });
}

void Ctx::msg_loop_write() noexcept {

  // build the reply from the request headers and content
  reply->build(ptr(), request->headers, request->content);

  net::async_write_msg(ptr(), [this, ctx = ptr()](error_code ec) mutable {
    if (!ec) {

      request->record_elapsed();

      // clear request and reply
      request.reset();
      reply.reset();

      msg_loop(); // prepare for next message

    } else { // error, teardown

      teardown();
    }
  });
}

Port Ctx::server_port(ports_t server_type) noexcept {
  Port port{0};

  switch (server_type) {

  case ports_t::AudioPort:
    audio_srv = Audio::start(io_ctx, shared_from_this());
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

void Ctx::set_live() noexcept { sessions->live(ptr()); }

void Ctx::teardown() noexcept {
  static constexpr csv fn_id{"teardown"};

  if (teardown_in_progress.exchange(true) == false) {
    const auto sar = active_remote;
    const auto sdi = dacp_id;
    const auto scn = client_name;

    INFO(module_id, fn_id, "completed '{}' {} {} \n", scn, sar, sdi);

    group_contains_group_leader = false;
    active_remote = 0;

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

    INFO(module_id, fn_id, "completed '{}' {} {} \n", scn, sar, sdi);
  }
}

} // namespace rtsp
} // namespace pierre