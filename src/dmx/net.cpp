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

#include "dmx/net.hpp"
#include "external/ArduinoJson.h"

using std::cerr, std::endl;

namespace this_thread = std::this_thread;
namespace asio = boost::asio;
namespace chrono = std::chrono;

using boost::asio::ip::udp;

namespace pierre {
namespace dmx {
Net::Net(io_context &io_ctx, const string_t &host, const string_t &port)
    : _host(host), _port(port), _socket(io_ctx) {

  resolver resolver(io_ctx);

  _endpoints = resolver.resolve(udp::v4(), _host, _port);

  _dest = *_endpoints.begin();

  _socket.open(udp::v4());
}

void Net::shutdown() {
  //  _socket.shutdown(socket::shutdown_type::shutdown_both);
  _socket.close();
}

bool Net::write(const UpdateInfo &info) {
  bool rc = true;
  error_code ec;

  auto data = info.frame;

  tx_data send_buff;
  send_buff.assign(data.size() + 256, 0x00);
  uint8_t *send_pos = send_buff.data();

  *send_pos++ = 0xd2; // mgaic lsb
  *send_pos++ = 0xc9; // magiv msb

  uint16_t frame_len = data.size();
  *send_pos++ = frame_len & 0x00ff;        // frame len lsb
  *send_pos++ = (frame_len & 0xff00) >> 8; // frame len msb

  auto data_end = data.cend();
  auto send_end = send_buff.data() + send_buff.size();

  for (auto it = data.cbegin(); it != data_end; it++, send_pos++) {
    if (send_pos == send_end) {
      break;
    }

    *send_pos = *it;
  }

  auto remaining = send_buff.data() + send_buff.size() - send_pos;
  stats.msgpack.bytes = serializeMsgPack(info.doc, send_pos, remaining);

  _socket.send_to(asio::buffer(send_buff), _dest, 0, ec);

  if (ec) {
    rc = false;
  }

  return rc;
}
} // namespace dmx
} // namespace pierre
