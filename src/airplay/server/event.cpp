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

#include "server/event.hpp"
#include "base/logger.hpp"
#include "session/event.hpp"

#include <fmt/format.h>

namespace pierre {
namespace airplay {
namespace server {

using namespace boost::system;

void Event::asyncLoop(const error_code ec_last) {
  // first things first, check ec_last passed in, bail out if needed
  if ((ec_last != errc::success) || !acceptor.is_open()) { // problem

    // don't highlight "normal" shutdown
    if ((ec_last.value() != errc::operation_canceled) &&
        (ec_last.value() != errc::resource_unavailable_try_again)) {
      INFO("AIRPLAY", "EVENT SERVER", "accept failed, error={}\n", ec_last.message());
    }
    // some kind of error occurred, simply close the socket
    [[maybe_unused]] error_code __ec;
    acceptor.close(__ec); // used error code overload to prevent throws

    return; // bail out
  }

  // this is the socket for the next accepted connection, store it in an
  // optional for the lamba
  socket.emplace(io_ctx);

  // since the io_ctx is wrapped in the optional and asyncLoop wants the actual
  // io_ctx we must deference or get the value of the optional
  acceptor.async_accept(*socket, [&](error_code ec) {
    if (ec == errc::success) {
      __infoAccept(socket->native_handle(), LOG_FALSE);

      // create the session passing all the options
      // notes
      //  1: we move the socket (value of the optional) to session
      //  2. we then start the session using the shared_ptr returned by Session::create()
      //  3. Session::start() must ensure the shared_ptr pointer is captured in the
      //     async lamba so it doesn't go out of scope

      // assemble the dependency injection and start the server
      session::Inject inject{.io_ctx = io_ctx, .socket = std::move(socket.value())};
      session::Event::start(inject);

      asyncLoop(ec); // schedule more work or gracefully exit
    }
  });
}

void Event::teardown() {
  // here we only issue the cancel to the acceptor.
  // the closing of the acceptor will be handled when
  // the error is caught by asyncLoop

  [[maybe_unused]] error_code __ec;
  acceptor.cancel(__ec);
}

} // namespace server
} // namespace airplay
} // namespace pierre
