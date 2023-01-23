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
#include "rtsp/request.hpp"
#include "saver.hpp"

#include <exception>

namespace pierre {

namespace rtsp {

Ctx::Ctx(io_context &io_ctx, tcp_socket sock) noexcept
    : io_ctx(io_ctx),                      //
      feedback_timer(io_ctx),              // detect absence of routine feedback messages
      sock(std::move(sock)),               //
      aes(std::shared_ptr<Aes>(new Aes())) //
{}

void Ctx::feedback_msg() noexcept {}

void Ctx::msg_loop() {
  static constexpr csv fn_id{"msg_loop"};

  try {

    for (;;) {
      Elapsed e; // track the elapsed time for this message

      std::shared_ptr<Request> request(new Request(shared_from_this()));
      auto fut = request->read_packet();

      if (auto msg = fut.get(); msg.empty() == false) break;

      Saver().format_and_write(*request);

      update_from(request->headers);

      Reply reply;
      reply.build(*request, shared_from_this());

      if (reply.empty()) { // throw on empty reply
        auto err = fmt::format("empty reply method={} path={}", request->headers.method(),
                               request->headers.path());

        throw std::runtime_error(err);
      }

      Saver().format_and_write(reply);

      // reply has content to send
      aes->encrypt(reply.packet()); // NOTE: noop until cipher exchange completed

      auto reply_buff = reply.buffer();
      asio::async_write(
          sock,       //
          reply_buff, //
          [=, this, s = ptr(), reply = std::move(reply)](error_code ec, size_t bytes) mutable {
            // write stats
            Stats::write(stats::RTSP_SESSION_MSG_ELAPSED, e.freeze());
            Stats::write(stats::RTSP_SESSION_TX_REPLY, static_cast<int64_t>(bytes));

            const auto msg = io::is_ready(sock, ec);

            if (!msg.empty()) {
              INFO(module_id, fn_id, "async_write failed, {}\n", msg);
              teardown();
            }
          });
    }
  } catch (std::exception &err) {
    INFO(module_id, fn_id, "{}\n", err.what());

    teardown();
  }
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

void Ctx::teardown() noexcept {

  INFO(module_id, "teardown", "active_remote={} dacp_id={} client_name='{}'\n", //
       active_remote, dacp_id, client_name);

  group_contains_group_leader = false;
  active_remote = 0;

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

  [[maybe_unused]] error_code ec;
  feedback_timer.cancel(ec);

  INFO(module_id, "teardown", "completed\n");
}

void Ctx::update_from(const Headers &h) noexcept {

  cseq = h.val<int64_t>(hdr_type::CSeq);
  active_remote = h.val<int64_t>(hdr_type::DacpActiveRemote);
  if (dacp_id.empty()) dacp_id = h.val<string>(hdr_type::DacpID);

  if (h.contains(hdr_type::XAppleProtocolVersion))
    proto_ver = h.val<int64_t>(hdr_type::XAppleProtocolVersion);

  if (h.contains(hdr_type::XAppleClientName) && client_name.empty())
    client_name = h.val(hdr_type::XAppleClientName);

  if (user_agent.empty()) user_agent = h.val(hdr_type::UserAgent);
}

} // namespace rtsp
} // namespace pierre