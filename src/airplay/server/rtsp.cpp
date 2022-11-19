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

#include "server/rtsp.hpp"
#include "base/logger.hpp"
#include "session/rtsp.hpp"

#include <fmt/format.h>

namespace pierre {
namespace airplay {
namespace server {

using namespace boost::asio;
using namespace boost::system;
using error_code = boost::system::error_code;

void Rtsp::asyncLoop(const error_code ec_last) {
  // first things first, check ec_last passed in, bail out if needed
  if ((ec_last != errc::success) || !acceptor.is_open()) { // problem

    // don't highlight "normal" shutdown
    if ((ec_last.value() != errc::operation_canceled) &&
        (ec_last.value() != errc::resource_unavailable_try_again)) {
      INFO("AIRPLAY", "SERVER", "accept failed, reason={}\n", ec_last.message());
    }
    // some kind of error occurred, simply close the socket
    [[maybe_unused]] error_code __ec;
    acceptor.close(__ec); // used error code overload to prevent throws

    return; // bail out
  }

  // this is the socket for the next accepted connection, store it in an
  // optional for the lamba
  socket.emplace(io_ctx);

  // since the io_ctx is wrapped in the optional and async_accept wants the actual
  // io_ctx we must deference or get the value of the optional
  acceptor.async_accept(*socket, [&](error_code ec) {
    if (ec == errc::success) {
      const auto &l = socket->local_endpoint();
      const auto &r = socket->remote_endpoint();

      INFO("AIRPLAY", server_id, "{}:{} -> {}:{} accepted\n", r.address().to_string(), r.port(),
           l.address().to_string(), l.port());

      // create the session passing all the options
      // notes
      //  1: we move the socket (value of the optional) to session
      //  2. we then start the session using the shared_ptr returned by Session::create()
      //  3. Session::start() must ensure the shared_ptr pointer is captured in the
      //     async lamba so it doesn't go out of scope

      // assemble the dependency injection and start the server
      const session::Inject inject{.io_ctx = io_ctx, // io_cty (used to create a local strand)
                                   .socket = std::move(socket.value())};

      session::Rtsp::start(inject);
    }

    asyncLoop(ec); // schedule more work or gracefully exit
  });
}

void Rtsp::teardown() {
  // here we only issue the cancel to the acceptor.
  // the closing of the acceptor will be handled when
  // the error is caught by asyncLoop

  [[maybe_unused]] error_code __ec;
  acceptor.cancel(__ec);
}

} // namespace server
} // namespace airplay
} // namespace pierre