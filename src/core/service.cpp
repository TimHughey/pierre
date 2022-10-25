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

#include "core/service.hpp"
#include "base/features.hpp"
#include "base/host.hpp"
#include "config/config.hpp"

#include <exception>
#include <fmt/format.h>

namespace pierre {

namespace shared {
std::optional<shService> __service;
std::optional<shService> &service() { return shared::__service; }
} // namespace shared

using namespace service;
using enum service::Key;

Service::Service() {
  auto host = Host::ptr();

  // stora calculated key/vals available from Host
  saveCalcVal(apAirPlayPairingIdentity, host->hwAddr());
  saveCalcVal(apDeviceID, host->deviceID());

  saveCalcVal(apGroupUUID, host->uuid());
  saveCalcVal(apSerialNumber, host->serialNum());

  saveCalcVal(ServiceName, string(Config::receiverName()));
  saveCalcVal(FirmwareVsn, Config::firmwareVersion());

  saveCalcVal(PublicKey, host->pk());

  addRegAndName();
  addSystemFlags();
  addFeatures();
}

void Service::addFeatures() {

  Features features;
  _features_val = features.ap2SetPeersX();

  constexpr auto mask32 = 0xffffffff;
  const uint64_t hi = (_features_val >> 32) & mask32;
  const uint64_t lo = _features_val & mask32;

  for (auto key : std::array{apFeatures, mdFeatures, plFeatures}) {
    switch (key) {
    case apFeatures:
    case mdFeatures: {
      auto str = fmt::format("{:#02X},{:#02X}", lo, hi);
      saveCalcVal(key, str);
    } break;

    case plFeatures: {
      auto str = fmt::format("{}", (int64_t)_features_val);
      saveCalcVal(key, str);
    } break;

    default:
      break;
    }
  }
}

void Service::addRegAndName() {
  // these must go into the calc map

  for (auto key : std::array{AirPlayRegNameType, RaopRegNameType}) {
    if (key == AirPlayRegNameType) {
      // _airplay._tcp service name is just the service
      saveCalcVal(key, fetchVal(ServiceName));
    }

    if (key == RaopRegNameType) {
      // _airplay._tcp service name is device_id@service
      auto device_id = fetchVal(apDeviceID);
      auto service_name = fetchVal(ServiceName);
      auto raop_service_name = fmt::format("{}@{}", device_id, service_name);

      saveCalcVal(key, raop_service_name);
    }
  }
}

void Service::addSystemFlags() {
  for (auto key : std::array{apSystemFlags, apStatusFlags, mdSystemFlags}) {
    auto str = fmt::format("{:#x}", _status_flags.val());

    saveCalcVal(key, str);
  }
}

void Service::receiverActive(bool on_off) {
  if (on_off == true) {
    _status_flags.rendering();

  } else {
    _status_flags.ready();
  }

  addSystemFlags(); // update the calculated key/val map
}

const service::KeyVal Service::fetch(const Key key) const {
  const auto &kv = _kvm.at(key);

  const auto &[__unused_key_str, val_str] = kv;

  // this is a constant (not calculated)
  if (val_str[0] != '*') {
    return kv;
  }

  // this is a calculated (saved val); get the saved value
  const auto &saved_kv = _kvm_calc.at(key);

  // destructure
  const auto &[saved_key_str, saved_val_str] = saved_kv;

  // build a standard key/val
  return KeyVal{saved_key_str, saved_val_str.get()};
}

const char *Service::fetchKey(const Key key) const {
  const auto &[key_str, __unused_val_str] = fetch(key);

  return key_str;
}

const char *Service::fetchVal(const Key key) const {
  const auto &[__unused_key_str, val_str] = fetch(key);

  return val_str;
}

service::sKeyValList Service::keyValList(Type service_type) const {
  // get the list of keys
  const auto &seq_list = _key_sequences.at(service_type);

  sKeyValList kv_list = std::make_shared<KeyValList>();

  // spin through the sequence list populating the key/list
  for (const auto key : seq_list) {
    auto kv = fetch(key);

    kv_list->emplace_back(kv);
  }

  return kv_list;
}

// return a key/val list for an adhoc list of keys
service::sKeyValList Service::keyValList(const KeySeq &want_keys) const {
  sKeyValList kv_list = std::make_shared<KeyValList>();

  for (const auto key : want_keys) {
    const auto &kv = fetch(key);
    kv_list->emplace_back(kv);
  }

  return kv_list;
}

const KeyVal Service::nameAndReg(Type type) const {
  switch (type) {
  case service::AirPlayTCP:
    return fetch(AirPlayRegNameType);

  case service::RaopTCP:
    return fetch(RaopRegNameType);
  }

  throw(std::runtime_error("bad service type"));
}

void Service::saveCalcVal(Key key, const string &val) {
  // get the key/val from the static map -- we need the key
  auto &[key_str, __unused] = _kvm.at(key);

  // create the new val str
  auto len = val.size() + 1; // plus null terminator
  service::ValStrCalc val_str(new char[len]{0});

  memcpy(val_str.get(), val.c_str(), len);

  // put the key/val tuple into the calc map
  _kvm_calc.insert_or_assign(key, service::KeyValCalc{key_str, val_str});
}

} // namespace pierre