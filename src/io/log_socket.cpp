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

#include "io/log_socket.hpp"

#include <exception>
#include <fmt/format.h>
#include <iterator>

namespace pierre {

const string io::log_socket_msg(error_code ec, tcp_socket &sock, const tcp_endpoint &r,
                                Elapsed e) noexcept {
  e.freeze();

  string msg;
  auto w = std::back_inserter(msg);

  auto open = sock.is_open();
  fmt::format_to(w, "{} ", open ? "[OPEN]" : "[CLSD]");

  try {
    if (open) {

      const auto &l = sock.local_endpoint();

      fmt::format_to(w, "{:>15}:{:<5} {:>15}:{:<5}",    //
                     l.address().to_string(), l.port(), //
                     r.address().to_string(), r.port());
    }

    if (ec != errc::success) fmt::format_to(w, " {}", ec.message());
  } catch (const std::exception &e) {

    fmt::format_to(w, "EXCEPTION {}", e.what());
  }

  if (e > 1us) fmt::format_to(w, " {}", e.humanize());

  return msg;
}

} // namespace pierre
