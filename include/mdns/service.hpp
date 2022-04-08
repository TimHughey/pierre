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
#include <string>
#include <unordered_map>

#include "core/host.hpp"

namespace pierre {
namespace mdns {

using namespace std;

typedef unordered_map<string, string> StringListMap;

enum ServiceType : int8_t { AirPlayTCP = 0, RaopTCP };

class Service {

public:
  typedef const string &csr;

  struct BaseData {
    sHost host;
    const string &service;
    const string &pk;
    const string &firmware_vsn;
  };

public:
  Service(ServiceType type);
  void build();
  auto name() const { return _service_name.c_str(); }
  auto regType() const { return _reg_type.c_str(); }
  static void setBaseData(const BaseData &data);

  const StringListMap &stringListMap() const { return _smap; }

protected:
  void addEntry(csr key, csr val);
  void addFeatures(csr key);

  // subclass API
  virtual void makeServiceName(sHost host, csr service, string &service_name){};
  virtual void populateStringList(sHost host){};
  virtual void setRegType(string &reg_type){};

private:
  ServiceType _stype;
  StringListMap _smap;
  string _service_name;
  string _reg_type;

  // the features code is a 64-bit number, but in the mDNS advertisement, the least significant 32
  // bit are given first for example, if the features number is 0x1C340405F4A00, it will be given as
  // features=0x405F4A00,0x1C340 in the mDNS string, and in a signed decimal number in the plist:
  // 496155702020608 this setting here is the source of both the plist features response and the
  // mDNS string.

  // const uint64_t _one = 1;
  // const uint64_t _mask = (_one << 17) | (_one << 16) | (_one << 15) | (_one << 50);

  // APX + Authentication4 (b14) with no metadata (see below)
  // const uint64_t airplay_features = 0x1C340405D4A00 & (~_mask);
  // std::array _feature_bits{9, 11, 18, 19, 30, 40, 41, 51};
  // uint64_t airplay_features = 0x00;

  uint64_t _features_val = 0x1C340445F8A00;
  string _features;

  // Advertised with mDNS and returned with GET /info, see
  // https://openairplay.github.io/airplay-spec/status_flags.html
  // 0x4: Audio cable attached, no PIN required (transient pairing)
  // 0x204: Audio cable attached, OneTimePairingRequired
  // 0x604: Audio cable attached, OneTimePairingRequired, device setup for Homekit access control
  uint32_t _status_flags = 0x04;

  static string _firmware_vsn;
  static sHost _host;
  static string _pk;
  static string _service_base;
};
} // namespace mdns
} // namespace pierre