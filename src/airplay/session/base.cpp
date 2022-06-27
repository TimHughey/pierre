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

#include "session/base.hpp"

#include <fmt/format.h>
#include <memory>
#include <unordered_map>
#include <utility>

namespace pierre {
namespace airplay {
namespace session {

using namespace boost::system;

Base::Base(const Inject &di, csv session_id)
    : socket(std::move(di.socket)), local_strand(di.io_ctx), session_id(session_id) {
  _acc.emplace(ACCUMULATE::RX, 0);
  _acc.emplace(ACCUMULATE::TX, 0);
}

Base::~Base() {
  if (false) { // debug
    constexpr auto f = FMT_STRING("{} {} shutdown handle={}\n");
    fmt::print(f, runTicks(), session_id, socket.native_handle());
  }

  [[maybe_unused]] error_code ec; // must use error_code overload to prevent throws
  socket.shutdown(tcp_socket::shutdown_both, ec);
  socket.close(ec);
}

bool Base::isReady(const error_code &ec) {
  auto rc = isReady();

  if (rc) {
    switch (ec.value()) {
      case errc::success:
        break;

      case errc::operation_canceled:
      case errc::resource_unavailable_try_again:
      case errc::no_such_file_or_directory:
        rc = false;
        break;

      default: {
        if (false) { // debug
          constexpr auto f = FMT_STRING("{} {} socket={} msg={}\n");
          fmt::print(f, runTicks(), sessionId(), socket.native_handle(), ec.message());
        }

        socket.shutdown(tcp_socket::shutdown_both);
        socket.close();
        rc = false;
      }
    }
  }

  return rc;
}

void Base::teardown() {
  [[maybe_unused]] error_code ec;

  socket.cancel(ec);
}

} // namespace session
} // namespace airplay
} // namespace pierre