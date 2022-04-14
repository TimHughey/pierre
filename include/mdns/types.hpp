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
#include <tuple>
#include <unordered_map>

namespace pierre {
namespace mdns {

enum ServiceType : int8_t { AirPlayTCP = 0, RaopTCP };

enum TxtKeys : int8_t {
  apFeatures = 0,
  mdFeatures,
  plFeatures,
  PublicKey,
  apGroupDiscoverableLeader,
  apGroupUUID,
  apAirPlayPairingIdentity,
  apAirPlayVsn,
  apSerialNumber,
  apManufacturer,
  apModel,
  FirmwareVsn,
  apSystemFlags,
  apProtocolVsn,
  apRequiredSenderFeatures,
  apDeviceID,
  apAccessControlLevel,
  mdAirPlayVsn,
  mdAirTunesProtocolVsn,
  mdSystemFlags,
  mdModel,
  mdMetadataTypes,
  mdEncryptTypes,
  mdDigestAuthKey,
  mdCompressionTypes,
  mdTransportTypes,
  apServiceName
};

typedef std::unordered_map<TxtKeys, const char *> TxtKeyVals;

typedef const std::string ServiceName;
typedef const std::string ServiceReg;

typedef std::unordered_map<std::string, std::string> KeyValMap;
typedef std::tuple<std::string, std::string> ServiceAndReg;

typedef std::vector<TxtKeys> TxtKeyOrder;
} // namespace mdns
} // namespace pierre