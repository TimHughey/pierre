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

Rtsp::Rtsp() noexcept
    : // create strand to serialize accepting connections
      accept_strand(asio::make_strand(io_ctx)),
      // use the created strand
      acceptor{accept_strand, tcp_endpoint(ip_tcp::v4(), LOCAL_PORT)},
      // worker strand (Audio, Event, Control)
      worker_strand(asio::make_strand(io_ctx)) //
{
  static constexpr csv fn_id{"init"};

  INFO_INIT("sizeof={:>5}\n", sizeof(Rtsp));

  boost::asio::socket_base::enable_connection_aborted opt(true);
  acceptor.set_option(opt);

  sessions = std::make_unique<rtsp::Sessions>();

  master_clock = std::make_unique<MasterClock>(io_ctx);

  desk = std::make_unique<Desk>(master_clock.get());

  std::jthread([this]() {
    // post work to the strand
    asio::post(io_ctx, [this]() {
      // allow cancellation of async_accept

      async_accept();
    });

    io_ctx.run();
  }).detach();
}

Rtsp::~Rtsp() noexcept {
  static constexpr csv fn_id{"shutdown"};

  io_ctx.stop();

  [[maybe_unused]] error_code ec;
  acceptor.close(ec);    //  prevent new connections
  sessions->close_all(); // close any existing connections

  desk.reset(); // shutdown desk (and friends)
  master_clock.reset();

  sessions.reset();

  INFO_AUTO("complete");
}

void Rtsp::async_accept() noexcept {
  static constexpr csv fn_id{"async_accept"};

  // note: must dereference the peer shared_ptr
  acceptor.async_accept([this](error_code ec, tcp_socket peer) mutable {
    Elapsed e;

    if ((ec == errc::success) && acceptor.is_open()) {
      desk->resume();

      auto ctx = std::make_shared<rtsp::Ctx>(worker_strand,      // worker strand
                                             std::move(peer),    //
                                             sessions.get(),     //
                                             master_clock.get(), //
                                             desk.get()          //
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
