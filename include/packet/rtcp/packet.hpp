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

#include <array>
#include <cstdint>
#include <memory>
#include <source_location>
#include <string_view>

namespace pierre {
namespace rtp {
namespace udp {
namespace control {

using src_loc = std::source_location;
typedef const src_loc csrc_loc;
typedef const std::string_view csv;
typedef const char *ccs;
typedef uint16_t SeqNum;
typedef uint8_t Type;

struct hdr {
  struct {
    uint8_t vpm = 0x00;      // vsn, padding, marker
    uint8_t type = 0x00;     // packet type
    uint16_t length = 0x000; // packet total length
  } full;

  void clear();
  uint8_t *data() { return (uint8_t *)&full; }
  size_t length() const { return full.length; }
  void loaded(size_t rx_bytes);

  size_t moreBytes() const { return size() - length(); }
  static constexpr size_t size() { return sizeof(full); }

  // header details
  bool marker() const { return ((full.vpm & 0x08) >> 4) == 0x01; }
  bool padding() const { return ((full.vpm & 0x20) >> 5) == 0x01; }

  uint8_t version() const { return (full.vpm & 0x0c) >> 6; }

  // misc debug
  void dump(csrc_loc loc = src_loc::current()) const;
  static ccs fnName(csrc_loc loc = src_loc::current()) { return loc.function_name(); }
};

class Packet {
private:
  static constexpr size_t STD_PACKET_SIZE = 128;
  typedef std::array<uint8_t, STD_PACKET_SIZE> Raw;

public:
  Packet() { clear(); }

  void clear() { _raw.fill(0x00); }
  uint8_t *data() { return (uint8_t *)_raw.data(); }

  void loaded(size_t rx_bytes);

  ccs raw() const { return (ccs)_raw.data(); }
  size_t size() const { return _size; }

  bool valid() const { return _valid; }
  const csv view() const { return csv(raw(), size()); }

  // misc debug
  static ccs fnName(src_loc loc = src_loc::current()) { return loc.function_name(); }

private:
  Raw _raw;
  size_t _size = 0;

  bool _valid = false;
};

} // namespace control
} // namespace udp
} // namespace rtp
} // namespace pierre