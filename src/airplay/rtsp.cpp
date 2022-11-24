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
#include "rtsp/session.hpp"
#include "stats/stats.hpp"

namespace pierre {

using namespace rtsp;

void Rtsp::async_loop(const error_code ec_last) noexcept {
  // first things first, check ec_last passed in, bail out if needed
  if ((ec_last != errc::success) || !acceptor.is_open()) { // problem

    // don't highlight "normal" shutdown
    if ((ec_last.value() != errc::operation_canceled) &&
        (ec_last.value() != errc::resource_unavailable_try_again)) {
      INFO("AIRPLAY", "SERVER", "accept failed, {}\n", ec_last.message());
    }
    teardown();

    return; // bail out
  }

  // this is the socket for the next accepted connection, store it in an
  // optional for the lamba
  sock_accept.emplace(io_ctx);

  // since the io_ctx is wrapped in the optional and async_accept wants the actual
  // io_ctx we must deference or get the value of the optional
  acceptor.async_accept(*sock_accept, [s = ptr(), e = Elapsed()](error_code ec) {
    Elapsed e2(e);
    e2.freeze();

    if (ec == errc::success) {
      const auto &l = s->sock_accept->local_endpoint();
      const auto &r = s->sock_accept->remote_endpoint();

      INFO("AIRPLAY", module_id, "{}:{} -> {}:{} accepted\n", r.address().to_string(), r.port(),
           l.address().to_string(), l.port());

      // create the session passing all the options
      // notes
      //  1: we move the socket (value of the optional) to session
      //  2. we then start the session using the shared_ptr returned by Session::create()
      //  3. Session::start() must ensure the shared_ptr pointer is captured in the
      //     async lamba so it doesn't go out of scope

      // assemble the dependency injection and start the server

      Stats::write(stats::RTSP_SESSION_CONNECT, e2.freeze());
      auto session = Session::create(s->io_ctx, std::move(s->sock_accept.value()));
      session->run(std::move(e));
    }

    s->async_loop(ec); // schedule more work or gracefully exit
  });
}

} // namespace pierre
