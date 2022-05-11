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
#include "session/event.hpp"

#include <fmt/format.h>

namespace pierre {
namespace airplay {
namespace server {

using namespace boost::system;

void Event::asyncLoop() {
  // capture the io_ctx in this optional for use when a request is accepted
  // call it socket since it will become one once accepted
  socket.emplace(io_ctx);

  // since the io_ctx is wrapped in the optional and async_accept wants the actual
  // io_ctx we must deference or get the value of the optional
  acceptor.async_accept(*socket, [&](error_code ec) {
    if (ec == errc::success) {
      // auto handle = (*socket).native_handle();
      // fmt::print("{} accepted connection, handle={}\n", fnName(), handle);

      // create the session passing all the options
      // notes
      //  1: we move the socket (value of the optional) to session
      //  2. we then start the session using the shared_ptr returned by Session::create()
      //  3. Session::start() must ensure it captures the shared_ptr
      //     to ensure the Session stays in scope

      // assemble the dependency injection and start the server
      auto inject = session::Inject{.socket = std::move(socket.value()),
                                    .conn = di.conn,
                                    .anchor = di.anchor,
                                    .host = di.host,
                                    .service = di.service,
                                    .mdns = di.mdns};
      session::Event::start(inject);
    }

    asyncLoop(ec); // schedule more work or gracefully exit
  });
}

void Event::asyncLoop(const error_code &ec) {
  if (ec == errc::success) {
    // no error condition, schedule more io_ctx work if the socket is open
    if (acceptor.is_open()) {
      asyncLoop();
    }
    return;
  }

  if ((ec.value() != errc::operation_canceled) &&
      (ec.value() != errc::resource_unavailable_try_again)) {
    auto fmt_str = FMT_STRING("{} accept connection failed, error={}\n");
    fmt::print(fmt_str, fnName(), ec.message());
  }

  acceptor.close();
}

} // namespace server
} // namespace airplay
} // namespace pierre
