/*
    Pierre - Custom Light Show via DMX for Wiss Landing
    Copyright (C) 2021  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#include <iostream>

#include "ArduinoJson.h"
#include "dmx/net.hpp"

using std::cerr, std::endl;

namespace this_thread = std::this_thread;
namespace asio = boost::asio;
namespace chrono = std::chrono;

using asio::buffer;
using boost::asio::ip::udp;

namespace pierre {
namespace dmx {
Net::Net(io_context &io_ctx, const string &host, const string &port)
    : _host(host), _port(port), _socket(io_ctx) {
  resolver resolver(io_ctx);

  _endpoints = resolver.resolve(udp::v6(), _host, _port);

  _dest = *_endpoints.begin();

  _socket.open(udp::v6());
}

void Net::shutdown() { _socket.close(); }

bool Net::write(Packet &packet) {
  bool rc = true;
  error_code ec;

  stats.msgpack.bytes = packet.msgLength();

  auto tx_buff = buffer(packet.txData(), packet.txDataLength());

  // only transmit the used portion of the packet
  _socket.send_to(tx_buff, _dest, 0, ec);

  if (ec) {
    rc = false;
  }

  return rc;
}
} // namespace dmx
} // namespace pierre
