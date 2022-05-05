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

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdint>
#include <exception>
#include <fmt/format.h>
#include <list>
#include <source_location>
#include <unordered_set>

#include "rtp/tcp/audio/server.hpp"
#include "rtp/tcp/audio/session.hpp"

namespace pierre {
namespace rtp {
namespace tcp {

using namespace boost::system;

void AudioServer::asyncAccept() {
  // capture the io_ctx in this optional for use when a request is accepted
  // call it socket since it will become one once accepted
  socket.emplace(io_ctx);

  // since the io_ctx is wrapped in the optional and asyncAccept wants the actual
  // io_ctx we must deference or get the value of the optional
  acceptor.async_accept(*socket, [&](error_code ec) {
    if (ec == errc::success) {
      announceAccept((*socket).native_handle());

      // create the session passing all the options
      // notes
      //  1: we move the socket (value of the optional) to session
      //  2. we then start the session using the shared_ptr returned by Session::create()
      //  3. Session::start() must ensure the shared_ptr pointer is captured in the
      //     async lamba so it doesn't go out of scope
      auto session =
          AudioSession::create({.new_socket = std::move(socket.value()), .audio_raw = audio_raw});

      session->asyncAudioBufferLoop();
    }

    asyncAccept(ec); // schedule more work or gracefully exit
  });
}

void AudioServer::asyncAccept(const error_code &ec) {
  if (ec == errc::success) {
    // no error condition, schedule more io_ctx work if the socket is open
    if (acceptor.is_open()) {
      asyncAccept();
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

} // namespace tcp
} // namespace rtp
} // namespace pierre