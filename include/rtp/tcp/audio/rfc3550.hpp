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

#pragma once

#include <cstdint>
#include <source_location>
#include <vector>

namespace pierre {
namespace rtp {
namespace tcp {
namespace audio {

class rfc3550 {
public:
  using RawData = std::vector<uint8_t>;
  using src_loc = std::source_location;
  typedef const char *ccs;

public:
  rfc3550(RawData &raw, size_t rx_bytes);
  rfc3550() = delete;

  bool reduceToFirstMarker();

  // typical getters
  uint16_t packetType() const { return type; }
  uint16_t seqNum() const { return seq_num; }
  uint32_t timestamp() const { return packet_timestamp; }

  // misc helpers
  void dumpHeader(const src_loc loc = src_loc::current());

private:
  bool findFirstMarker();

  ccs fnName(const src_loc loc = src_loc::current()) const { return loc.function_name(); }

private:
  RawData &_raw;
  const size_t _rx_bytes;
  size_t _tossed_bytes = 0;

  uint8_t marker_bit = 0x00;
  uint8_t type = 0x00;
  uint16_t seq_num = 0x00;
  uint8_t v2 = 0x00;
  uint32_t packet_timestamp = 0x00;

  static constexpr uint8_t STANDARD = 0x60;
  static constexpr uint8_t RESEND = 0x56;
};

} // namespace audio
} // namespace tcp
} // namespace rtp
} // namespace pierre
