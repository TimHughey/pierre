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

#include "mdns/service/airplay.hpp"

namespace pierre {
namespace mdns {

AirPlay::AirPlay() : Service(AirPlayTCP) {}

void AirPlay::makeServiceName(sHost host, csr service, string &service_name) {
  service_name = service;
}

// enp7s0 IPv4 Roost Phazon _airplay._tcp local
// enp7s0 IPv4 Roost Phazon _airplay._tcp local
// hostname = [Sonos - 542A1BE7B4B3.local]
// address = [192.168.2.11]
// port = [7000]
// txt =  ["pk=acb17ea9c839598988dcb6996313f43d361ac7208ac42c76e2f9d7f661cacc0d"
//          "gcgl=0"
//          "gid=54:2A:1B:E7:B4:B3"
//          "pi=54:2A:1B:E7:B4:B3"
//          "srcvers=366.0"
//          "protovers=1.1"
//          "serialNumber=54-2A-1B-E7-B4-B3:5"
//          "manufacturer=Sonos"
//          "model=Amp"
//          "flags=0x4"
//          "fv=p20.67.1-27100"
//          "rsf=0x0"
//          "features=0x445F8A00,0x1C340"
//          "deviceid=54:2A:1B:E7:B4:B3"
//          "acl=0"]

void AirPlay::populateStringList(sHost host) {
  addFeatures("features");
  addEntry("gcgl", "0");                       // /info 1
  addEntry("gid", host->hwAddr());             // /info 1
  addEntry("pi", host->uuid());                // /info 1
  addEntry("srcvers", "366.0");                // /info 1
  addEntry("protovers", "1.1");                // /info 1
  addEntry("serialNumber", host->serialNum()); // /info 1
  addEntry("manufacturer", "Hughey");          // /info step 1
  addEntry("model", "Lights By Pierre");       // /info step 1
  addEntry("flags", "0x4");                    // /info step 1
  addEntry("rsf", "0x0");                      // /info step 1
  addEntry("deviceid", host->hwAddr());        // /info step 1
  addEntry("acl", "0");                        //  /info step 1

  // /info 1 - pk = device_id
  // /info 1 - features
  // /info 1 - statusFlags
  // /info 1 - deviceID
  // /info 1 - pi
  // /info 1 - name
  // /info 1 - model
  // /info 1 Content-Type: application/x-apple-binary-plist
  // /info 1 response code = 200
}

void AirPlay::setRegType(string &reg_type) { reg_type = "_airplay._tcp"; }

} // namespace mdns
} // namespace pierre