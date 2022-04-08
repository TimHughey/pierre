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

#include <fmt/format.h>

#include "mdns/service/raop.hpp"

namespace pierre {
namespace mdns {

Raop::Raop() : Service(RaopTCP) {}

void Raop::makeServiceName(sHost host, csr service, string &service_name) {
  service_name = fmt::format("{}@{}", host->deviceId(), service);
}

// enp7s0 IPv4 542A1BE7B4B3 @Roost Phazon _raop._tcp local
// enp7s0 IPv4 542A1BE7B4B3 @Roost Phazon _raop._tcp local
//   hostname = [Sonos - 542A1BE7B4B3.local]
//   address = [192.168.2.11]
//   port = [7000]
//   txt = ["pk=acb17ea9c839598988dcb6996313f43d361ac7208ac42c76e2f9d7f661cacc0d"
//           "vs=366.0"
//           "vn=65537"
//           "tp=UDP"
//           "sf=0x4"
//           "am=Amp"
//           "md=0,1,2"
//           "fv=p20.67.1-27100"
//           "ft=0x445F8A00,0x1C340"
//           "et=0,4"
//           "da=true"
//           "cn=0,1"]

void Raop::populateStringList(sHost host) {
  addFeatures("ft");
  addEntry("vs", "366.0");
  addEntry("vn", "65537");
  addEntry("tp", "UDP");
  addEntry("sf", "0x4");
  addEntry("am", "Lights By Pierre");
  addEntry("md", "0,1,2");
  addEntry("et", "0,4");
  addEntry("da", "true");
  addEntry("cn", "0,1");
}

void Raop::setRegType(string &reg_type) { reg_type = "_raop._tcp"; }

} // namespace mdns
} // namespace pierre