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

#include <array>
#include <boost/asio.hpp>
#include <fmt/format.h>
#include <iterator>
#include <source_location>
#include <string>

#include "anchor/client.hpp"

namespace pierre {
namespace anchor {
namespace client {

using namespace boost::asio;
using namespace boost::system;

void Session::sendCtrlMsg(const string &shm_name, const string &msg) {
  if (socket.is_open() == false) {
    socket.async_connect(endpoint, [shm_name, pending_msg = msg, this](error_code ec) {
      if (ec == errc::success) {
        sendCtrlMsg(shm_name, pending_msg); // recurse

      } else {
        fmt::print("{} connect failed what={}\n", fnName(), ec.what());
        return;
      }
    });
  }

  _wire.clear();                              // clear buffer
  auto to = std::back_inserter(_wire);        // iterator to format to
  fmt::format_to(to, "{} {}", shm_name, msg); // full msg is 'shm_name msg'
  _wire.emplace_back(0x00);                   // nqptp wants a null terminated string
  auto buff = buffer(_wire);                  // lightweight buffer wrapper for async_*

  socket.async_send_to(buff, endpoint, [&](error_code ec, size_t tx_bytes) {
    if (false) {
      constexpr auto f = FMT_STRING("{} wrote ctrl msg bytes={:>03} ec={}\n");
      fmt::print(f, fnName(), tx_bytes, ec.what());
    }
  });
}

} // namespace client
} // namespace anchor
} // namespace pierre
