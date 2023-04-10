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
#include "ctx.hpp"
#include "desk/desk.hpp"
#include "frame/master_clock.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"
#include "lcs/stats.hpp"

#include <algorithm>
#include <boost/asio/post.hpp>
#include <thread>

namespace pierre {

namespace shared {
std::unique_ptr<Rtsp> rtsp;
}

Rtsp::Rtsp(asio::io_context &io_ctx) noexcept
    : // create a strand to accept rtsp connections
      accept_strand(asio::make_strand(io_ctx)),
      // use the created strand
      acceptor{accept_strand, tcp_endpoint(ip_tcp::v4(), LOCAL_PORT)},
      sessions(std::make_unique<rtsp::Sessions>()),        //
      master_clock(std::make_unique<MasterClock>(io_ctx)), //
      desk(std::make_unique<Desk>(master_clock.get()))     //
{
  static constexpr csv fn_id{"init"};
  INFO_INIT("sizeof={}\n", sizeof(Rtsp));

  // post work to the strand
  asio::post(accept_strand, std::bind(&Rtsp::async_accept, this));

  // add a new thread to the io_ctx for the acceptor
  std::jthread([io_ctx = std::ref(io_ctx)]() { io_ctx.get().run(); }).detach();
}

Rtsp::~Rtsp() noexcept {
  [[maybe_unused]] error_code ec;
  acceptor.close(ec);    //  prevent new connections
  sessions->close_all(); // close any existing connections

  desk.reset(); // shutdown desk (and friends)
  master_clock.reset();

  sessions.reset();
}

void Rtsp::async_accept() noexcept {
  static constexpr csv fn_id{"async_accept"};

  // note: must dereference the peer shared_ptr
  acceptor.async_accept([this](error_code ec, tcp_socket peer) mutable {
    Elapsed e;

    if ((ec == errc::success) && acceptor.is_open()) {
      desk->resume();

      auto ctx = std::make_shared<rtsp::Ctx>( //
          std::move(peer),                    //
          sessions.get(),                     //
          master_clock.get(),                 //
          desk.get()                          //
      );

      sessions->add(ctx);
      ctx->run();

      Stats::write(stats::RTSP_SESSION_CONNECT, e.freeze());

      async_accept(); // schedule the next accept
    } else if (ec != errc::operation_canceled) {
      INFO_AUTO("failed, {}\n", ec.message());
    }
  });
}

} // namespace pierre
