/*
Pierre - Custom audio processing for light shows at Wiss Landing
Copyright (C) 2022  Tim Hughey

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <cstdint>
#include <memory>
#include <uuid/uuid.h>

namespace pierre {

typedef std::array<uint8_t, 6> HwAddrBytes;

// forward decl for typedef
class Host;

typedef std::shared_ptr<Host> sHost;

class Host : std::enable_shared_from_this<Host> {
public:
  [[nodiscard("discarding only shared_ptr")]] static sHost create() {
    // Not using std::make_shared<T> because the c'tor is private.
    return sHost(new Host());
  }

  const std::string &deviceId() const { return _device_id; }
  sHost getPtr() { return shared_from_this(); }
  const std::string &hwAddr() const { return _hw_addr; }
  const HwAddrBytes &hwAddrBytes() const { return _hw_addr_bytes; }
  const std::string &serialNum() const { return _serial_num; }
  const std::string &uuid() const { return _uuid; }

private:
  Host(); // private, only accessible via shared_ptr

  void createHostIdentifiers();
  void createUUID();
  bool findHardwareAddr(HwAddrBytes &dest);

public:
  std::string _device_id;
  std::string _hw_addr;
  HwAddrBytes _hw_addr_bytes{0};
  std::string _serial_num;
  std::string _uuid;
};

} // namespace pierre