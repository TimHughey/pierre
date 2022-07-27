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

#include "base/typical.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace pierre {

typedef std::array<uint8_t, 6> HwAddrBytes;
typedef std::string Hostname;
typedef std::string IpAddr;
typedef std::vector<IpAddr> IpAddrs;
typedef std::array<uint8_t, 32> PkBytes;

class Host;
typedef std::shared_ptr<Host> shHost;

namespace shared {
std::optional<shHost> &host();
} // namespace shared

// Host Command Information
class Host : public std::enable_shared_from_this<Host> {
public:
  static shHost init() { return shared::host().emplace(new Host()); }
  static shHost ptr() { return shared::host().value()->shared_from_this(); }
  static void reset() { shared::host().reset(); }

public:
  Host();

  // general API
  ccs deviceID() const { return _device_id.c_str(); }
  ccs hwAddr() const { return _hw_addr.c_str(); }

  // IP address(es) of this host
  const HwAddrBytes &hwAddrBytes() const { return _hw_addr_bytes; }
  const Hostname &hostname() const { return _hostname; }
  const IpAddrs &ipAddrs() const { return _ip_addrs; }

  const string pk() const; // without 0x prefix

  ccs serialNum() const { return _serial_num.c_str(); }

  // UUID for this host
  ccs uuid() const { return _uuid.c_str(); }

private:
  void createHostIdentifiers();
  void createPublicKey();
  void createUUID();
  void discoverIPs();

  void initCrypto();
  bool findHardwareAddr(HwAddrBytes &dest);

public:
  Hostname _hostname;
  string _device_id;
  string _hw_addr;
  HwAddrBytes _hw_addr_bytes{0};
  IpAddrs _ip_addrs;
  PkBytes _pk_bytes{0};
  string _serial_num;

  string _uuid;

private:
  static constexpr auto _gcrypt_vsn = "1.5.4";
};

} // namespace pierre