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
typedef std::string IpAddr;
typedef std::vector<IpAddr> IpAddrs;
typedef std::array<uint8_t, 32> PkBytes;
typedef std::string string;
typedef const char *cc_str;

// forward decl for typedef
class Host;
typedef std::shared_ptr<Host> sHost;

class Host : public std::enable_shared_from_this<Host> {
public:
  // NOTE: creation of the shared instance
  [[nodiscard]] static sHost create(const string &firmware_vsn,
                                    const string &service_name) {
    // Not using std::make_shared<T> because the c'tor is private.
    return sHost(new Host(firmware_vsn, service_name));
  }

  sHost getPtr() { return shared_from_this(); }

  // general API

  cc_str deviceID() const { return _device_id.c_str(); }
  cc_str firmware_vsn() const { return _firmware_vsn.c_str(); };

  cc_str hwAddr() const { return _hw_addr.c_str(); }
  const HwAddrBytes &hwAddrBytes() const { return _hw_addr_bytes; }

  const IpAddrs &ipAddrs() const { return _ip_addrs; }

  // default format is without 0x prefix
  const string pk(const char *format = "{:02x}") const;
  cc_str serialNum() const { return _serial_num.c_str(); }
  cc_str serviceName() const { return _service_name.c_str(); }
  cc_str uuid() const { return _uuid.c_str(); }

private:
  // private, only accessible via shared_ptr
  Host(const string &_firmware_vsn, const string &service_name);

  void createHostIdentifiers();
  void createPublicKey();
  void createUUID();
  void discoverIPs();

  void initCrypto();
  bool findHardwareAddr(HwAddrBytes &dest);

public:
  string _app_name;
  string _firmware_vsn;
  string _service_name;

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