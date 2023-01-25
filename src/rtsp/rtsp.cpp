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
#include "lcs/config.hpp"
#include "lcs/logger.hpp"
#include "lcs/stats.hpp"
#include "mdns/features.hpp"
#include "replies/info.hpp"

#include <fmt/ostream.h>
#include <latch>
#include <mutex>

namespace pierre {

// static class data
std::shared_ptr<Rtsp> Rtsp::self;

void Rtsp::async_accept() noexcept {
  static constexpr csv fn_id{"async_accept"};

  // this is the socket for the next accepted connection. the socket
  // is move only so we create it as a shared_ptr for capture by the lamdba
  auto peer = std::make_shared<tcp_socket>(io_ctx);

  // note: must dereference the peer shared_ptr
  acceptor.async_accept(*peer, [this, s = ptr(), peer = peer](error_code ec) {
    Elapsed e;

    if ((ec == errc::success) && acceptor.is_open()) {

      // ensure Desk is running, it may be shutdown due to idle timeout
      Desk::init();

      const auto &r = peer->remote_endpoint();

      const auto msg = io::log_socket_msg(ec, *peer, r, e);
      INFO(module_id, fn_id, "{}\n", msg);

      { // lock and unlock sessions as quickly as possible
        std::unique_lock lck(sessions_mtx, std::defer_lock);
        lck.lock();
        auto ctx = sessions.emplace_back(new rtsp::Ctx(io_ctx, std::move(peer)));

        ctx->run();
      }

      async_accept(); // schedule the next accept

      Stats::write(stats::RTSP_SESSION_CONNECT, e.freeze());
    } else {
      INFO(module_id, fn_id, "failed, {}\n", ec.message());
    }
  });
}

void Rtsp::init() noexcept {
  static constexpr csv threads_path("rtsp.threads");

  self = std::shared_ptr<Rtsp>(new Rtsp());

  const auto thread_count = config_val<int>(threads_path, 4);

  // create shared_ptrs to avoid spurious data races
  auto latch = std::make_shared<std::latch>(thread_count);

  for (auto n = 0; n < thread_count; n++) {
    self->threads.emplace_back([n, s = self->ptr(), latch]() mutable {
      name_thread("rtsp", n);

      // we want a syncronized start of all threads
      latch->arrive_and_wait();
      latch.reset();

      s->io_ctx.run();
    });
  }

  latch->wait();

  self->async_accept();

  INFO(module_id, "init", "sizeof={} threads={}/{} features={:#x}\n", sizeof(Rtsp),
       std::ssize(self->threads), thread_count, Features().ap2Default());
}

void Rtsp::shutdown() noexcept { // static
  if (self.use_count() > 1) {

    [[maybe_unused]] error_code ec;
    self->acceptor.close(ec);
    auto &sessions = self->sessions;

    std::for_each(sessions.begin(), sessions.end(), [](auto ctx) {
      [[maybe_unused]] error_code ec;
      ctx->sock->shutdown(tcp_socket::shutdown_both, ec);
      ctx->sock->close(ec);
    });

    self->guard.reset(); // allow the io_ctx to complete when other work is finished

    for (auto &t : self->threads) {
      INFO(module_id, "shutdown", "joining {} self.use_count({})\n", fmt::streamed(t.get_id()),
           self.use_count());
      t.join();
    }

    // reset the shared_ptr to ourself.  provided no other shared_ptrs exist
    // when this function returns this object falls out of scope
    self.reset();

    INFO(module_id, "shutdown", "complete self.use_count({})\n", self.use_count());
  }
}

} // namespace pierre
