/*
    devs/dmx/headunit.hpp - Ruth Dmx Head Unit Device
    Copyright (C) 2020  Tim Hughey

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

#ifndef pierre_dmx_headunit_device_hpp
#define pierre_dmx_headunit_device_hpp

#include "dmx/packet.hpp"
#include "local/types.hpp"

namespace pierre {
namespace lightdesk {

class HeadUnit {
public:
  HeadUnit() : _address(0), _frame_len(0) {
    // support headunits that do not use the DMX frame
  }

  HeadUnit(uint address, size_t frame_len)
      : _address(address), _frame_len(frame_len){};
  virtual ~HeadUnit() {}

  virtual void framePrepare() = 0;
  virtual void frameUpdate(dmx::Packet &packet) = 0;

  virtual void dark() {}

  float fps() const { return 44.0f; }

protected:
  const uint _address;
  const uint _frame_len;
};
} // namespace lightdesk
} // namespace pierre

#endif
