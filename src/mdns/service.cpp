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

#include "fmt/format.h"

#include "mdns/service.hpp"

namespace pierre {
namespace mdns {

Service::Service(ServiceType type) : _stype(type) {
  // create, setup all the common string list entries
}

void Service::addEntry(const string &key, const string &val) { _smap.try_emplace(key, val); }
void Service::addFeatures(csr key) {
  constexpr auto mask32 = 0xffffffff;
  const uint64_t hi = (_features_val >> 32) & mask32;
  const uint64_t lo = _features_val & mask32;

  // airplay features are a 64-bit value
  // mDNS advertisment representation is in three parts:
  //   1. least significant 32-bits
  //   2. a comma
  //   3. most significant 32-bits
  addEntry(key, fmt::format("{:#02X},{:#02X}", lo, hi));
}

void Service::build() {

  setRegType(_reg_type);
  makeServiceName(_host, _service_base, _service_name);

  addEntry("fv", _firmware_vsn);
  addEntry("pk", _pk); // private key

  populateStringList(_host);

  // no reason to hold on the shared pointer
  // _host.reset();
}

void Service::setBaseData(const BaseData &base) {
  _host = base.host;
  _service_base = base.service;
  _pk = base.pk;
  _firmware_vsn = base.firmware_vsn;
}

// provide storage for static class members
sHost Service::_host;
string Service::_firmware_vsn;
string Service::_pk;
string Service::_service_base;

} // namespace mdns
} // namespace pierre