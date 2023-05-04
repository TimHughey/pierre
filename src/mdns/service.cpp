// Pierre
// Copyright (C) 2022 Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#include "service.hpp"
#include "base/host.hpp"
#include "base/logger.hpp"
#include "base/uint8v.hpp"
#include "base/uuid.hpp"
#include "features.hpp"
#include "pair/pair.h"

#include <array>
#include <exception>

namespace pierre {

Service::Service(const string &receiver, const string &build_vsn, string &msg) noexcept
    : receiver(receiver), build_vsn(build_vsn) //
{
  const auto host = Host();

  update_key_val(txt_opt::apAirPlayPairingIdentity, host.hw_address());
  update_key_val(txt_opt::apDeviceID, host.device_id());

  const auto uuid = UUID(); // create UUID for this host (only once)

  update_key_val(txt_opt::apGroupUUID, uuid);
  update_key_val(txt_opt::apSerialNumber, host.serial_num());

  update_key_val(txt_opt::ServiceName, receiver);
  update_key_val(txt_opt::FirmwareVsn, build_vsn);

  // create the public key
  uint8v pk_bytes(32, 0x00);
  pair_public_key_get(PAIR_SERVER_HOMEKIT, pk_bytes.data(), host.hw_address().data());
  update_key_val(txt_opt::PublicKey, pk_bytes);

  Features features;
  const auto features_val = features.ap2SetPeersX();

  constexpr auto mask32 = 0xffffffff;
  const uint64_t hi = (features_val >> 32) & mask32;
  const uint64_t lo = features_val & mask32;

  for (auto key : std::array{txt_opt::apFeatures, txt_opt::mdFeatures, txt_opt::plFeatures}) {
    switch (key) {
    case apFeatures:
    case mdFeatures: {
      auto str = fmt::format("{:#02X},{:#02X}", lo, hi);
      update_key_val(key, str);
    } break;

    case plFeatures: {
      const auto str = fmt::format("{}", features_val);
      update_key_val(key, str);
    } break;

    default:
      break;
    }
  }

  for (auto opt : std::array{txt_opt::AirPlayRegNameType, txt_opt::RaopRegNameType}) {
    if (opt == txt_opt::AirPlayRegNameType) {
      const auto &[key, val] = key_val(txt_opt::ServiceName);

      // _airplay._tcp service name is just the service
      update_key_val(opt, val);
    }

    if (opt == RaopRegNameType) {
      // _airplay._tcp service name is device_id@service
      const auto &device_id = key_val(txt_opt::apDeviceID);
      const auto &service_name = key_val(txt_opt::ServiceName);
      const auto raop_service_name = fmt::format("{}@{}", device_id.second, service_name.second);

      update_key_val(opt, raop_service_name);
    }
  }

  update_system_flags();

  msg = fmt::format("mdns::Service sizeof={:>5} uuid={}", sizeof(Service), uuid);
}

lookup_map_t Service::lookup_map{
    // comment to make IDE sorting easy
    {AirPlayRegNameType, std::make_pair("_airplay._tcp", string())},
    {apAccessControlLevel, std::make_pair("acl", "0")},
    {apAirPlayPairingIdentity, std::make_pair("pi", string())},
    {apAirPlayVsn, std::make_pair("srcvers", "366.0")},
    {apDeviceID, std::make_pair("deviceid", string())},
    {apFeatures, std::make_pair("features", string())},
    {apGroupDiscoverableLeader, std::make_pair("gcgl", "0")},
    {apGroupUUID, std::make_pair("gid", string())},
    {apManufacturer, std::make_pair("manufacturer", "Hughey")},
    {apModel, std::make_pair("model", "Lights By Pierre")},
    {apProtocolVsn, std::make_pair("protovers", "1.1")},
    {apRequiredSenderFeatures, std::make_pair("rsf", "0x0")},
    {apSerialNumber, std::make_pair("serialNumber", string())},
    {apStatusFlags, std::make_pair("statusFlags", string())},
    {apSystemFlags, std::make_pair("flags", string())},
    {FirmwareVsn, std::make_pair("fv", string())},
    {mdAirPlayVsn, std::make_pair("vs", "366.0")},
    {mdAirTunesProtocolVsn, std::make_pair("vn", "1.1")},
    {mdCompressionTypes, std::make_pair("cn", "0,4")},
    {mdDigestAuthKey, std::make_pair("da", "true")},
    {mdDigestAuthKey, std::make_pair("da", "true")},
    {mdEncryptTypes, std::make_pair("et", "0,4")},
    {mdEncryptTypes, std::make_pair("et", "0,4")},
    {mdFeatures, std::make_pair("ft", string())},
    {mdFeatures, std::make_pair("ft", string())},
    {mdMetadataTypes, std::make_pair("md", "0,1,2")},
    {mdModel, std::make_pair("am", "Lights By Pierre")},
    {mdSystemFlags, std::make_pair("sf", string())},
    {mdTransportTypes, std::make_pair("tp", "UDP")},
    {plFeatures, std::make_pair("features", string())},
    {PublicKey, std::make_pair("pk", string())},
    {RaopRegNameType, std::make_pair("_raop._tcp", string())},
    {ServiceName, std::make_pair("name", string())},
    // extra comma to make IDE sorting easy
};

service_txt_map Service::service_map{
    {AirPlayTCP,
     service_def{.type = AirPlayTCP,
                 .order = {PublicKey, apFeatures, apGroupDiscoverableLeader, apGroupUUID,
                           apAirPlayPairingIdentity, apProtocolVsn, apSerialNumber, apManufacturer,
                           apModel, apSystemFlags, apRequiredSenderFeatures, apDeviceID,
                           apAccessControlLevel, apAirPlayVsn}}},

    {RaopTCP, //
     service_def{.type = RaopTCP,
                 .order = {PublicKey, mdAirPlayVsn, mdAirTunesProtocolVsn, mdTransportTypes,
                           mdSystemFlags, mdModel, mdMetadataTypes, mdEncryptTypes, mdDigestAuthKey,
                           mdCompressionTypes}}

    }};

} // namespace pierre