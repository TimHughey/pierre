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
#include "logger/logger.hpp"
#include "config/config.hpp"
#include "mdns/features.hpp"
#include "replies/info.hpp"
#include "session.hpp"
#include "stats/stats.hpp"

#include <latch>
#include <mutex>

namespace pierre {

// static class data
std::shared_ptr<Rtsp> Rtsp::self;

void Rtsp::async_accept() noexcept {
  // this is the socket for the next accepted connection, store it in an
  // optional for the lamba
  sock_accept.emplace(io_ctx);

  // since the io_ctx is wrapped in the optional and async_accept wants the actual
  // io_ctx we must deference or get the value of the optional
  acceptor.async_accept(*sock_accept, [s = ptr(), e = Elapsed()](error_code ec) {
    Elapsed e2(e);
    e2.freeze();

    if ((ec == errc::success) && s->acceptor.is_open()) {
      if (config_debug("rtsp.server")) {
        const auto &r = s->sock_accept->remote_endpoint();

        const auto msg = io::log_socket_msg("ACCEPT", ec, s->sock_accept.value(), r, e2);
        INFO(module_id, "LISTEN", "{}\n", msg);
      }

      // create the session passing all the options
      // notes
      //  1: we move the socket (value of the optional) to session
      //  2. we then start the session using the shared_ptr returned by Session::create()
      //  3. Session::start() must ensure the shared_ptr pointer is captured in the
      //     async lamba so it doesn't go out of scope

      Stats::write(stats::RTSP_SESSION_CONNECT, e2.freeze());
      auto session = rtsp::Session::create(s->io_ctx, std::move(s->sock_accept.value()));
      session->run(std::move(e));

      s->async_accept(); // schedule the next accept
    } else {

      INFO(module_id, "RTSP", "accept failed, {}\n", ec.message());
      shutdown();
    }
  });
}

void Rtsp::init() noexcept {
  static constexpr csv threads_path("rtsp.threads");

  self = std::shared_ptr<Rtsp>(new Rtsp());

  const auto thread_count = config_val<int>(threads_path, 4);

  // create shared_ptrs to avoid spurious data races
  auto once_flag = std::make_shared<std::once_flag>();
  auto latch = std::make_shared<std::latch>(thread_count);

  for (auto n = 0; n < thread_count; n++) {
    self->threads.emplace_back([n, s = self->ptr(), latch, once_flag]() mutable {
      name_thread("rtsp", n);

      // do special startup tasks once
      std::call_once(*once_flag, []() { rtsp::Info::init(); });

      // we want a syncronized start of all threads
      latch->arrive_and_wait();
      s->io_ctx.run();
    });
  }

  latch->wait();

  self->async_accept();

  if (debug_init())
    INFO(module_id, "INIT", "sizeof={} threads={}/{} features={:#x}\n", sizeof(Rtsp),
         std::ssize(self->threads), thread_count, Features().ap2Default());
}

} // namespace pierre
