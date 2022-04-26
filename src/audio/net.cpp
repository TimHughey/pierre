/*
    Pierre - Custom Light Show for Wiss Landing
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

#include "net.hpp"

#include <iostream>

using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;

namespace pierre {
namespace audio {

RawOut::Client::Client(io_context &io_ctx) : _socket(io_ctx) { _socket.open(udp::v4()); }

void RawOut::Client::send(const RawPacket &data, endpoint &end_pt) {
  error_code ec;
  const socket::message_flags flags = 0;

  auto bytes_sent = _socket.send_to(buffer(data), end_pt, flags, ec);

  if (bytes_sent != data.size()) {
    cerr << end_pt << " " << ec << endl;
  }
}

RawOut::RawOut(const string &dest, const string &port) {
  udp::resolver resolver(_io_ctx);

  auto result = *resolver.resolve(udp::v4(), dest, port, _ec).begin();
  if (!_ec) {
    _dest_endpoint = result;

    cerr << "sending raw audio data to " << dest;
    cerr << ":" << port << endl;

  } else {
    cerr << dest << ": " << _ec << endl;
  }
}

shared_ptr<jthread> RawOut::run() {
  auto t = make_shared<jthread>([this]() { this->stream(); });

  return t;
}

void RawOut::stream() {
  auto packet_pos = _packet.begin();
  auto packet_end = _packet.cend();

  while (_shutdown == false) {
    const auto entry = pop();             // actual queue entry
    const auto byte_count = entry->bytes; // num of actual bytes
    const auto &bytes = entry->raw;       // raw bytes

    const auto last_byte = bytes.cbegin() + byte_count;

    // copy the incoming bytes to the outgoing network packet.  when the packet
    // is full send it.
    for (auto byte = bytes.cbegin(); byte <= last_byte; byte++) {
      if (packet_pos == packet_end) {
        // packet is full
        _client.send(_packet, _dest_endpoint);

        // reset the packet position to the beginning to continue copying
        // remaining bytes and the next sample packet
        packet_pos = _packet.begin();
      }

      *packet_pos++ = *byte;
    }
  }
}

} // namespace audio
} // namespace pierre
