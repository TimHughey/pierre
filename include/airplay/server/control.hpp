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

#include "common/ss_inject.hpp"
#include "common/typedefs.hpp"
#include "server/base.hpp"

#include <array>
#include <memory>

// #include "rtp/udp/control/packet.hpp"

namespace pierre {
namespace airplay {
namespace server {

namespace control {

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

// back to namespace server

class Control : public Base {
public:
  // create the Control
  Control(const Inject &di)
      : io_ctx(di.io_ctx),                                   // io_ctx
        socket(io_ctx, udp_endpoint(ip_udp::v6(), ANY_PORT)) // create socket and endpoint
  {
    _wire.clear();
  }

  void asyncLoop(const error_code ec_last = DEF_ERROR_CODE) override;

  control::hdr &hdr() { return _hdr; }
  uint8_t *hdrData() { return _hdr.data(); }
  size_t hdrSize() const { return _hdr.size(); }

  // ensure the server is started and return the local endpoint port
  uint16_t localPort() override { return socket.local_endpoint().port(); }

  void teardown() override;

private:
  void asyncRestOfPacket();

  bool isReady() const { return socket.is_open(); };
  bool isReady(const error_code &ec, src_loc loc = src_loc::current());

  void nextBlock();

  control::Packet &wire() { return _wire; }

private:
  // order dependent
  io_context &io_ctx;
  udp_socket socket;

  // latest sender endpoint
  udp_endpoint remote_endpoint;

  control::Packet _wire;
  control::hdr _hdr;
  uint64_t _rx_bytes = 0;
  uint64_t _tx_bytes = 0;
};

} // namespace server
} // namespace airplay
} // namespace pierre
