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

#include <vector>

#include "core/service.hpp"
#include "core/service/types.hpp"

namespace pierre {

using namespace core;
using namespace core::service;
using enum Key;

static const char *__calc = "*";

// static definitions for Service

KeyValMap Service::_kvm{{apAccessControlLevel, KeyVal{"acl", "0"}},
                        {apAirPlayPairingIdentity, KeyVal{"pi", __calc}},
                        {apAirPlayVsn, KeyVal{"srcvers", "366.0"}},
                        {apDeviceID, KeyVal{"deviceid", __calc}},
                        {apFeatures, KeyVal{"features", __calc}},
                        {apGroupDiscoverableLeader, KeyVal{"gcgl", "0"}},
                        {apGroupUUID, KeyVal{"gid", __calc}},
                        {apManufacturer, KeyVal{"manufacturer", "Hughey"}},
                        {apModel, KeyVal{"model", "Lights By Pierre"}},
                        {apProtocolVsn, KeyVal{"protovers", "1.1"}},
                        {apRequiredSenderFeatures, KeyVal{"rsf", "0x0"}},
                        {apSerialNumber, KeyVal{"serialNumber", __calc}},
                        {apSystemFlags, KeyVal{"flags", __calc}},
                        {mdAirPlayVsn, KeyVal{"vs", "366.0"}},
                        {mdAirTunesProtocolVsn, KeyVal{"vn", "1.1"}},
                        {mdCompressionTypes, KeyVal{"cn", "0,4"}},
                        {mdDigestAuthKey, KeyVal{"da", "true"}},
                        {mdEncryptTypes, KeyVal{"et", "0,4"}},
                        {mdFeatures, KeyVal{"ft", __calc}},
                        {mdMetadataTypes, KeyVal{"md", "0,1,2"}},
                        {mdModel, KeyVal{"am", "Lights By Pierre"}},
                        {mdSystemFlags, KeyVal{"sf", __calc}},
                        {mdTransportTypes, KeyVal{"tp", "UDP"}},
                        {plFeatures, KeyVal{"features", __calc}},
                        {FirmwareVsn, KeyVal{"fv", __calc}},
                        {PublicKey, KeyVal{"pk", __calc}},
                        {ServiceName, KeyVal{"name", __calc}},
                        {AirPlayRegNameType, KeyVal{"_airplay._tcp", __calc}},
                        {RaopRegNameType, KeyVal{"_raop._tcp", __calc}}};

KeySequences Service::_key_sequences{
    KeySeq{PublicKey, apFeatures, apGroupDiscoverableLeader, apGroupUUID,
           apAirPlayPairingIdentity, apProtocolVsn, apSerialNumber,
           apManufacturer, apModel, apSystemFlags, apRequiredSenderFeatures,
           apDeviceID, apAccessControlLevel},

    KeySeq{PublicKey, mdAirPlayVsn, mdAirTunesProtocolVsn, mdTransportTypes,
           mdSystemFlags, mdModel, mdMetadataTypes, mdEncryptTypes,
           mdDigestAuthKey, mdCompressionTypes}};

KeyValMapCalc Service::_kvm_calc;

} // namespace pierre