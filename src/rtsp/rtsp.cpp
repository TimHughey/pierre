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
#include "base/threads.hpp"
#include "ctx.hpp"
#include "desk/desk.hpp"
#include "frame/master_clock.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"
#include "lcs/stats.hpp"
#include "mdns/features.hpp"
#include "replies/info.hpp"

#include <algorithm>
#include <latch>
#include <mutex>

namespace pierre {

static auto cfg_path(csv key_path) noexcept { return toml::path(Rtsp::module_id).append(key_path); }

static const auto threads_path() noexcept { return cfg_path("threads"); }

Rtsp::Rtsp() noexcept
    : acceptor{io_ctx, tcp_endpoint(ip_tcp::v4(), LOCAL_PORT)},
      sessions(std::make_unique<rtsp::Sessions>()),     //
      master_clock(std::make_unique<MasterClock>()),    //
      desk(std::make_unique<Desk>(master_clock.get())), //
      thread_count(config()->at(threads_path()).value_or(4)),
      shutdown_latch(std::make_shared<std::latch>(thread_count)) //
{
  INFO(module_id, "init", "sizeof={} features={:#x}\n", sizeof(Rtsp), Features().ap2Default());

  // once io_ctx is started begin accepting connections
  // queuing accept to the io_ctx also serves as the work guard
  asio::post(io_ctx, std::bind(&Rtsp::async_accept, this));

  // create shared_ptrs to avoid spurious data races
  auto startup_latch = std::make_shared<std::latch>(thread_count);
  shutdown_latch = std::make_shared<std::latch>(thread_count);

  for (auto n = 0; n < thread_count; n++) {
    std::jthread([this, n, startup_latch = startup_latch,
                  shutdown_latch = shutdown_latch]() mutable {
      const auto thread_name = name_thread("rtsp", n);

      // we want a syncronized start of all threads
      startup_latch->arrive_and_wait();
      startup_latch.reset();

      INFO(module_id, "init", "thread {}\n", thread_name);

      io_ctx.run();

      shutdown_latch->count_down();
      INFO(module_id, "shutdown", "thread {}\n", thread_name);
    }).detach();
  }

  startup_latch->wait(); // wait for all threads to start
}

Rtsp::~Rtsp() noexcept {

  static constexpr csv fn_id{"shutdown"};
  INFO(module_id, fn_id, "requested\n");

  [[maybe_unused]] error_code ec;
  acceptor.close(ec);    //  prevent new connections
  sessions->close_all(); // close any existing connections
  sessions.reset();
  desk.reset(); // shutdown desk (and friends)
  master_clock.reset();

  INFO(module_id, fn_id, "waiting for threads, stopped={}\n", io_ctx.stopped());
  shutdown_latch->wait(); // caller waits for all threads to finish

  INFO(module_id, fn_id, "io_ctx stopped={}\n", io_ctx.stopped());
}

void Rtsp::async_accept() noexcept {
  static constexpr csv fn_id{"async_accept"};

  // this is the socket for the next accepted connection. the socket
  // is move only so we create it as a shared_ptr for capture by the lamdba
  auto peer = std::make_shared<tcp_socket>(io_ctx);

  // note: must dereference the peer shared_ptr
  acceptor.async_accept(*peer, [this, peer = peer](error_code ec) {
    Elapsed e;

    if ((ec == errc::success) && acceptor.is_open()) {
      // ensure Desk is running, it may be shutdown due to idle timeout
      desk->resume();

      const auto &r = peer->remote_endpoint();

      const auto msg = io::log_socket_msg(ec, *peer, r, e);
      INFO(module_id, fn_id, "{}\n", msg);

      auto ctx = sessions->create(io_ctx, peer, master_clock.get(), desk.get());

      ctx->run();

      async_accept(); // schedule the next accept

      Stats::write(stats::RTSP_SESSION_CONNECT, e.freeze());
    } else if (ec != errc::operation_canceled) {
      INFO(module_id, fn_id, "failed, {}\n", ec.message());
    }
  });
}

} // namespace pierre
