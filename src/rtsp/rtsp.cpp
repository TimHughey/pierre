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

#include "rtsp.hpp"
#include "base/elapsed.hpp"
#include "base/logger.hpp"
#include "base/stats.hpp"
#include "ctx.hpp"
#include "desk/desk.hpp"
#include "frame/master_clock.hpp"

#include <algorithm>
#include <boost/asio/post.hpp>
#include <thread>

namespace pierre {

Rtsp::Rtsp(asio::io_context &app_io_ctx, Desk *desk)
    : // save the supplied io_ctx
      io_ctx(app_io_ctx),
      // capture the Desk pointer (we need it for sessions contexts)
      desk(desk),
      // use the created strand
      acceptor{app_io_ctx, tcp_endpoint(ip_tcp::v4(), LOCAL_PORT)} //
{
  static constexpr csv fn_id{"init"};

  INFO_INIT("sizeof={:>5}\n", sizeof(Rtsp));

  boost::asio::socket_base::enable_connection_aborted opt(true);
  acceptor.set_option(opt);

  async_accept();
}

Rtsp::~Rtsp() noexcept {
  static constexpr csv fn_id{"shutdown"};

  [[maybe_unused]] error_code ec;
  acceptor.close(ec); //  prevent new connections
  close_all();

  INFO_AUTO("complete");
}

void Rtsp::async_accept() noexcept {
  static constexpr csv fn_id{"async_accept"};

  acceptor.async_accept([this](error_code ec, tcp_socket peer) mutable {
    Elapsed e;

    if ((ec == errc::success) && acceptor.is_open()) {

      sessions.emplace_back(rtsp::Ctx::create(std::move(peer), this, desk)).get();

      Stats::write(stats::RTSP_SESSION_CONNECT, e.freeze());

      async_accept(); // schedule the next accept
    } else if (ec != errc::operation_canceled) {
      INFO_AUTO("failed, {}\n", ec.message());
    }
  });
}

void Rtsp::close_all() noexcept {
  constexpr auto fn_id{"close_all"sv};

  if (sessions.size()) {
    std::for_each(sessions.begin(), sessions.end(), [](const auto &ctx) { ctx->force_close(); });
  }

  INFO_AUTO("closed sessions={}\n", sessions.size());
}

void Rtsp::set_live(rtsp::Ctx *live_ctx) noexcept {
  static constexpr csv fn_id{"set_live"};

  INFO_AUTO("{}\n", *live_ctx);

  if (!sessions.empty()) {

    std::erase_if(sessions, [&](const auto &ctx) mutable {
      auto rc = false;

      if (ctx && (live_ctx->active_remote != ctx->active_remote)) {
        INFO( "freeing", "{}\n", *ctx);

        ctx->force_close();
        rc = true;
      }

      return rc;
    });
  }
}

} // namespace pierre
