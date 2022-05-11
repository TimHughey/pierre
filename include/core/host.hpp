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

#include "core/config.hpp"
#include "core/typedefs.hpp"

#include <cstdint>
#include <memory>
#include <uuid/uuid.h>
#include <vector>

namespace pierre {

typedef std::array<uint8_t, 6> HwAddrBytes;
typedef std::string IpAddr;
typedef std::vector<IpAddr> IpAddrs;
typedef std::array<uint8_t, 32> PkBytes;

// Host Command Information
class Host {
public:
  struct Inject {
    const Config &cfg;
  };

public:
  Host(const Inject &di);

  // general API
  csv appName() const { return serviceName(); }

  ccs deviceID() const { return _device_id.c_str(); }

  ccs firmwareVerson() const { return cfg.firmwareVersion().c_str(); };

  ccs hwAddr() const { return _hw_addr.c_str(); }

  // IP address(es) of this host
  const HwAddrBytes &hwAddrBytes() const { return _hw_addr_bytes; }
  const IpAddrs &ipAddrs() const { return _ip_addrs; }

  const string pk(const char *format = "{:02x}") const; // without 0x prefix

  ccs serialNum() const { return _serial_num.c_str(); }
  ccs serviceName() const { return cfg.serviceName().c_str(); }

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
  // config provider
  const Config &cfg;

  string _app_name;

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