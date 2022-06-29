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

#pragma once

#include "base/typical.hpp"
#include "base/uint8v.hpp"

#include <ArduinoJson.h>
#include <vector>

namespace pierre {
namespace packet {

class DMX {
public:
  DMX();

  inline auto frameData() { return frame.data(); }
  inline char *msgPack() { return p.payload + p.len.frame; };
  inline const char *msgPackConst() { return (const char *)msgPack(); }
  inline auto msgLength() const { return p.len.msg; }

  inline JsonObject &rootObj() { return root; }

  uint8_t *txData();
  size_t txDataLength() const { return p.len.packet; }

public:
  uint8v frame;

private:
  typedef StaticJsonDocument<384> MsgPackDoc;

private:
  MsgPackDoc doc;
  JsonObject root;

  // packet contents wrapped to ensure contiguous memory
  struct {
    uint16_t magic = 0xc9d2;
    struct {
      uint16_t packet = 0;
      uint16_t frame = 0;
      uint16_t msg = 0;
    } len;
    char payload[768] = {0};
  } p;
};

} // namespace packet
} // namespace pierre
