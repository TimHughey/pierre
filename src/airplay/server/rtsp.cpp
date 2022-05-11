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
#include "session/rtsp.hpp"

#include <fmt/format.h>

namespace pierre {
namespace airplay {
namespace server {

using namespace boost::asio;
using namespace boost::system;
using error_code = boost::system::error_code;

void Rtsp::asyncLoop() {
  // capture the io_ctx in this optional for use when a request is accepted
  // call it socket since it will become one once accepted
  socket.emplace(io_ctx);

  // since the io_ctx is wrapped in the optional and async_accept wants the actual
  // io_ctx we must deference or get the value of the optional
  acceptor.async_accept(*socket, [&](error_code ec) {
    if (ec == errc::success) {
      __infoAccept(SERVER_ID, socket->native_handle(), LOG_TRUE);

      // create the session passing all the options
      // notes
      //  1: we move the socket (value of the optional) to session
      //  2. we then start the session using the shared_ptr returned by Session::create()
      //  3. Session::start() must ensure the shared_ptr pointer is captured in the
      //     async lamba so it doesn't go out of scope

      auto inject = session::Inject{.socket = std::move(socket.value()),
                                    .conn = di.conn,
                                    .anchor = di.anchor,
                                    .host = di.host,
                                    .service = di.service,
                                    .mdns = di.mdns};

      session::Rtsp::start(inject);
    }

    asyncLoop(ec); // schedule more work or gracefully exit
  });
}

void Rtsp::asyncLoop(const error_code &ec) {
  if ((ec == errc::success) && acceptor.is_open()) { // all good
    asyncLoop();                                     // schedule more work
    return;
  }

  if ((ec.value() != errc::operation_canceled) &&
      (ec.value() != errc::resource_unavailable_try_again)) {
    constexpr auto f = FMT_STRING("{} {} accept failed, error={}\n");
    fmt::print(f, runTicks(), SERVER_ID, ec.message());
  }

  acceptor.cancel();
  acceptor.close();
}

} // namespace server
} // namespace airplay
} // namespace pierre