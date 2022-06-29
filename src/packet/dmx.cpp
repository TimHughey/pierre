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

#include "packet/dmx.hpp"

#include <ArduinoJson.hpp>
#include <boost/array.hpp>
#include <boost/asio.hpp>

namespace asio = boost::asio;
using asio::buffer;
using asio::buffer_copy;

namespace pierre {
namespace packet {

DMX::DMX() {
  frame.assign(64, 0x00); // support only frames of 64 bytes
  root = doc.to<JsonObject>();
}

uint8_t *DMX::txData() {
  auto data = buffer(frame); // dmx frame to tx

  // wrap the packet frame in a buffer to use buffer_copy
  auto frame = buffer(&(p.payload), data.size());
  buffer_copy(frame, data);   // copy the frame data
  p.len.frame = frame.size(); // record the length of the frame

  // calculate msg max len, start position in the payload then
  // serialize directly into the payload
  auto msg_start = p.payload + p.len.frame;
  auto msg_max = sizeof(p.payload) - p.len.frame;
  p.len.msg = serializeMsgPack(doc, msg_start, msg_max);

  // finalize the length of the tx packet
  p.len.packet = sizeof(DMX) - sizeof(p.payload) // fixed fields
                 + p.len.frame + p.len.msg;      // length of frame and msg

  // wrap the tx packet in a buffer of the correct size to avoid tx of
  // unused bytes
  return (uint8_t *)&p;
}

} // namespace packet
} // namespace pierre
