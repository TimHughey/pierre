// Pierre
// Copyright (C) 2022 Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#include "zservice.hpp"
#include "base/logger.hpp"

#include <exception>
#include <fmt/format.h>

namespace pierre {

ZeroConf::ZeroConf(Details d) noexcept
    : hostname(d.hostname),  //
      name_net(d.name_net),  // service host name
      _address(d.address),   // service host port
      _type(d.type),         // address family (IPv4 or IPv6)
      _port(d.port),         // service port
      _protocol(d.protocol), // protocol
      _txt_list(d.txt_list)  // txt records
{
  static constexpr char at{'@'};

  auto delim_pos = name_net.find_first_of(at);

  if (delim_pos != std::string::npos) {
    auto mac_end = name_net.begin();
    std::advance(mac_end, delim_pos - 1);

    name_mac.assign(name_net.begin(), mac_end);

    auto short_begin = name_net.begin();
    std::advance(short_begin, delim_pos + 1);

    _name_short.assign(short_begin, name_net.end());
  } else {
    INFO(module_id, "CONSTRUCT", "unable to find delim={} in name_net={}\n", at, name_net);
  }
}

string ZeroConf::inspect() const noexcept {
  string msg;
  auto w = std::back_inserter(msg);

  fmt::format_to(w, "{} {} {} {} {} {}:{} TXT: ", _type, hostname, name_net, _name_short, _protocol,
                 _address, _port);

  std::for_each(_txt_list.begin(), _txt_list.end(),
                [w = std::back_inserter(msg)](const zc::Txt &txt) {
                  fmt::format_to(w, "{}=", txt.key());

                  if (txt.number()) {
                    fmt::format_to(w, "{} (number) ", txt.val<uint32_t>());
                  } else {
                    fmt::format_to(w, "{} ", txt.val());
                  }
                });

  return msg;
}

} // namespace pierre