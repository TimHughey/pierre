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
#include <source_location>
#include <string_view>
#include <uuid/uuid.h>
#include <vector>

#include "core/config.hpp"

namespace pierre {

using string_view = std::string_view;

typedef std::array<uint8_t, 6> HwAddrBytes;
typedef std::string IpAddr;
typedef std::vector<IpAddr> IpAddrs;
typedef std::array<uint8_t, 32> PkBytes;
typedef std::string string;
typedef const char *cc_str;
typedef const std::string_view csv;

// forward decl for typedef
class Host;
typedef std::shared_ptr<Host> sHost;
typedef std::shared_ptr<Host> shHost;

class Host : public std::enable_shared_from_this<Host> {
public:
  using src_loc = std::source_location;
  typedef const char *ccs;

public:
  // NOTE: creation of the shared instance
  [[nodiscard]] static sHost create(const Config &cfg) {
    // Not using std::make_shared<T> because the c'tor is private.
    __instance__ = sHost(new Host(cfg));

    return __instance__->shared_from_this();
  }

  [[nodiscard]] static shHost use() { return __instance__->shared_from_this(); }

  sHost getSelf() { return shared_from_this(); }

  // general API
  csv appName() const { return _app_name; }
  cc_str deviceID() const { return _device_id.c_str(); }
  cc_str firmwareVerson() const { return cfg.firmwareVersion().c_str(); };

  cc_str hwAddr() const { return _hw_addr.c_str(); }
  const HwAddrBytes &hwAddrBytes() const { return _hw_addr_bytes; }
  const IpAddrs &ipAddrs() const { return _ip_addrs; }

  // default format is without 0x prefix
  const string pk(const char *format = "{:02x}") const;
  cc_str serialNum() const { return _serial_num.c_str(); }
  cc_str serviceName() const { return cfg.serviceName().c_str(); }

  cc_str uuid() const { return _uuid.c_str(); }

private:
  // private, only accessible via shared_ptr
  Host(const Config &cfg);

  void createHostIdentifiers();
  void createPublicKey();
  void createUUID();
  void discoverIPs();

  void initCrypto();
  bool findHardwareAddr(HwAddrBytes &dest);

  // misc helpers
  ccs fnName(src_loc loc = src_loc::current()) const { return loc.function_name(); }

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

  static shHost __instance__;

private:
  static constexpr auto _gcrypt_vsn = "1.5.4";
};

} // namespace pierre