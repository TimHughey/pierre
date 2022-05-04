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

// #pragma once

// #include <boost/asio/buffer.hpp>
// #include <cstdint>
// #include <source_location>
// #include <string.h>
// #include <string_view>
// #include <vector>

// namespace pierre {
// namespace rtp {
// namespace tcp {
// namespace audio {

// class Packet {
// public:
//   enum Types : uint8_t { Unknown = 0, Standard, Resend };

// public:
//   using src_loc = std::source_location;
//   typedef const std::string_view csv;
//   typedef const char *ccs;
//   typedef uint16_t SeqNum;
//   typedef uint8_t Type;

// public:
//   Packet() { reset(); }

//   auto buffer() { return boost::asio::dynamic_buffer(_packet); }
//   bool loaded(size_t rx_bytes);

//   ccs raw() const { return (ccs)_packet.data(); }
//   void reset();
//   size_t size() const { return _packet.size(); }

//   SeqNum sequenceNum() const { return _seq_num; }
//   uint32_t timestamp() const { return _timestamp; }
//   Type type() const { return _type; }
//   bool valid() const { return _valid; }
//   const csv view() const { return csv(raw(), size()); }

//   // misc helpers, debug, etc.
//   ccs fnName(src_loc loc = src_loc::current()) const { return loc.function_name(); }

// private:
//   std::vector<uint8_t> _packet;

//   SeqNum _seq_num = 0;
//   Type _type = 0;
//   uint32_t _timestamp = 0;
//   bool _valid = false;

//   size_t _rx_bytes = 0;

//   static constexpr size_t STD_PACKET_SIZE = 2048;
// };

// } // namespace audio
// } // namespace tcp
// } // namespace rtp
// } // namespace pierre