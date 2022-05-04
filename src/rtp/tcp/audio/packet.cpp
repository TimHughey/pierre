// //  Pierre - Custom Light Show for Wiss Landing
// //  Copyright (C) 2022  Tim Hughey
// //
// //  This program is free software: you can redistribute it and/or modify
// //  it under the terms of the GNU General Public License as published by
// //  the Free Software Foundation, either version 3 of the License, or
// //  (at your option) any later version.
// //
// //  This program is distributed in the hope that it will be useful,
// //  but WITHOUT ANY WARRANTY; without even the implied warranty of
// //  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// //  GNU General Public License for more details.
// //
// //  You should have received a copy of the GNU General Public License
// //  along with this program.  If not, see <http://www.gnu.org/licenses/>.
// //
// //  https://www.wisslanding.com

// #include <boost/asio/buffer.hpp>
// #include <fmt/format.h>

// #include "rtp/tcp/audio/packet.hpp"
// #include "rtp/tcp/audio/rfc3550.hpp"

// namespace pierre {
// namespace rtp {
// namespace tcp {
// namespace audio {

// bool Packet::loaded(size_t rx_bytes) {
//   auto rfc = rfc3550(_packet, rx_bytes);
//   _valid = rfc.reduceToFirstMarker();

//   if (_valid) {
//     _type = rfc.packetType();
//     _seq_num = rfc.seqNum();
//     _timestamp = rfc.timestamp();
//   }

//   return _valid;
// }

// void Packet::reset() {
//   _packet.clear();
//   // _packet.resize(STD_PACKET_SIZE);

//   _seq_num = 0;
//   _type = 0;
//   _valid = false;
// }

// // misd helpers, debug, etc.

// } // namespace audio
// } // namespace tcp
// } // namespace rtp
// } // namespace pierre