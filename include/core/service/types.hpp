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

#include <array>
#include <cstdint>
#include <list>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace pierre {
namespace core {
namespace service {

enum Type : int8_t { AirPlayTCP = 0, RaopTCP };

enum Key : int8_t {
  apAccessControlLevel = 0,
  apAirPlayPairingIdentity,
  apAirPlayVsn,
  apDeviceID,
  apFeatures,
  apGroupDiscoverableLeader,
  apGroupUUID,
  apManufacturer,
  apModel,
  apProtocolVsn,
  apRequiredSenderFeatures,
  apSerialNumber,
  apSystemFlags,
  FirmwareVsn,
  mdAirPlayVsn,
  mdAirTunesProtocolVsn,
  mdCompressionTypes,
  mdDigestAuthKey,
  mdEncryptTypes,
  mdFeatures,
  mdMetadataTypes,
  mdModel,
  mdSystemFlags,
  mdTransportTypes,
  plFeatures,
  PublicKey,
  AirPlayRegNameType,
  RaopRegNameType,
  ServiceName
};

// constant values, safe to use when retrieved from container
typedef const char *KeyStr;
typedef const char *ValStr;

// calculated values, safe to use when retrieved from container
typedef std::shared_ptr<char[]> ValStrCalc;

// static key/val pairs
typedef std::tuple<KeyStr, ValStr> KeyVal;
typedef std::unordered_map<Key, KeyVal> KeyValMap;

// calculated / cached key/val pairs
typedef std::tuple<KeyStr, ValStrCalc> KeyValCalc;
typedef std::unordered_map<Key, KeyValCalc> KeyValMapCalc;

// single key sequence
typedef std::vector<Key> KeySeq;

// sequences of keys for AirPlayTCP, RaopUDP
typedef std::array<KeySeq, 2> KeySequences;

typedef std::list<KeyVal> KeyValList;
typedef std::shared_ptr<KeyValList> sKeyValList;

} // namespace service
} // namespace core
} // namespace pierre