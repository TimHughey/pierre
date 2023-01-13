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
#include "desk/desk.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"
#include "lcs/stats.hpp"
#include "mdns/features.hpp"
#include "replies/info.hpp"
#include "session.hpp"

#include <fmt/ostream.h>
#include <latch>
#include <mutex>

namespace pierre {

// static class data
std::shared_ptr<Rtsp> Rtsp::self;

void Rtsp::async_accept() noexcept {
  static constexpr csv fn_id{"async_accept"};

  // this is the socket for the next accepted connection. the socket
  // is move only so we store it in an optional and move it to session in
  // the lambda
  sock_accept.emplace(io_ctx);

  // since the io_ctx is wrapped in the optional and async_accept wants the actual
  // io_ctx we must deference or get the value of the optional
  acceptor.async_accept(*sock_accept, [s = ptr(), e = Elapsed()](error_code ec) {
    Elapsed e2(e);
    e2.freeze();

    if ((ec == errc::success) && s->acceptor.is_open()) {

      const auto &r = s->sock_accept->remote_endpoint();

      const auto msg = io::log_socket_msg(ec, s->sock_accept.value(), r, e2);
      INFO(module_id, fn_id, "{}\n", msg);

      // create the session passing all the options
      // notes
      //  1: we move the socket (value of the optional) to session
      //  2. we then start the session using the shared_ptr returned by Session::create()
      //  3. Session::start() must ensure the shared_ptr pointer is captured in the
      //     async lamba so it doesn't go out of scope

      Stats::write(stats::RTSP_SESSION_CONNECT, e2.freeze());
      auto session = rtsp::Session::create(s->io_ctx, std::move(s->sock_accept.value()));

      // ensure Desk is running, it may be shutdown due to idle timeout
      Desk::init();

      session->run(std::move(e));

      s->session_storage.emplace(std::move(session));

      s->async_accept(); // schedule the next accept
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
  auto once_flag = std::make_shared<std::once_flag>();
  auto latch = std::make_shared<std::latch>(thread_count);

  for (auto n = 0; n < thread_count; n++) {
    self->threads.emplace_back([n, s = self->ptr(), latch, once_flag]() mutable {
      name_thread("rtsp", n);

      // do special startup tasks once
      std::call_once(*once_flag, []() { rtsp::Info::init(); });

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

    if (self->session_storage.has_value()) self->session_storage.value()->shutdown();

    self->guard.reset(); // allow the io_ctx to complete when other work is finished

    for (auto &t : self->threads) {
      INFO(module_id, "shutdown", "joining {} self.use_count({})\n", t.get_id(), self.use_count());
      t.join();
    }

    // reset the shared_ptr to ourself.  provided no other shared_ptrs exist
    // when the above post completes we will be deallocated
    self.reset();

    INFO(module_id, "shutdown", "complete self.use_count({})\n", self.use_count());
  }
}

} // namespace pierre
