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

#include <exception>
#include <fmt/core.h>
#include <fmt/format.h>
#include <memory>
#include <plist/plist++.h>

#include "core/service.hpp"
namespace pierre {
namespace core {

using namespace service;
using enum service::Key;

Service::Service(sHost host) {
  // stora calculated key/vals available from Host
  saveCalcVal(apAirPlayPairingIdentity, host->hwAddr());
  saveCalcVal(apDeviceID, host->hwAddr());

  saveCalcVal(apGroupUUID, host->uuid());
  saveCalcVal(apSerialNumber, host->serialNum());

  saveCalcVal(ServiceName, host->serviceName());
  saveCalcVal(FirmwareVsn, host->firmware_vsn());

  addPublicKey(host);
  addRegAndName(host);
  addSystemFlags();
  addFeatures();
}

void Service::addRegAndName(auto host) {
  // these must go into the calc map

  for (auto key : std::array{AirPlayRegNameType, RaopRegNameType}) {
    if (key == AirPlayRegNameType) {
      // _airplay._tcp service name is just the service
      saveCalcVal(key, host->serviceName());
    }

    if (key == RaopRegNameType) {
      // _airplay._tcp service name is device_id@service
      auto service_name = fmt::format("{}@{}", host->deviceID(), host->serviceName());
      saveCalcVal(key, service_name);
    }
  }
}

void Service::addFeatures() {
  constexpr uint64_t bit = 1;
  uint64_t mask64 = (bit << 17) | (bit << 16) | (bit << 15) | (bit << 50);
  // APX + Authentication4 (b14) with no metadata
  _features_val = 0x1C340405D4A00 & (~mask64);

  constexpr auto mask32 = 0xffffffff;
  const uint64_t hi = (_features_val >> 32) & mask32;
  const uint64_t lo = _features_val & mask32;

  for (auto key : array{apFeatures, mdFeatures, plFeatures}) {
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

void Service::addPublicKey(auto host) {
  auto pk = host->pk();

  saveCalcVal(PublicKey, pk);
}

void Service::addSystemFlags() {
  for (auto key : array{apSystemFlags, mdSystemFlags}) {
    auto str = fmt::format("{:#x}", _system_flags);

    saveCalcVal(key, str);
  }
}

const KeyVal Service::fetch(const Key key) const {
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

sKeyValList Service::keyValList(Type service_type) const {
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

const KeyVal Service::nameAndReg(Type type) const {
  switch (type) {
    case AirPlayTCP:
      return fetch(AirPlayRegNameType);

    case RaopTCP:
      return fetch(RaopRegNameType);
  }

  throw(runtime_error("bad service type"));
}

void Service::saveCalcVal(Key key, const string &val) {
  // get the key/val from the static map -- we need the key
  auto &[key_str, __unused] = _kvm.at(key);

  // create the new val str
  auto len = val.size();
  ValStrCalc val_str(new char[len]);
  memcpy(val_str.get(), val.c_str(), len);

  // put the key/val tuple into the calc map
  _kvm_calc.emplace(key, KeyValCalc{key_str, val_str});
}

// const KeyValMap Service::advertiseMap(ServiceType type) const {
//   KeyValMap map;

//   const auto &order = (type == AirPlayTCP) ? apKeyOrder : mdKeyOrder;

//   // first add all the AirPlay2 key/val
//   for (const auto &key : order) {
//     auto calc = txtVals.end();
//     auto txt_entry = txtVals.find(key);

//     if (txt_entry == calc) {
//       // entry is not a constant, calculate
//       const auto &key_str = txtKeyVals.at(key);
//       const auto val = calcTxtVal(key);
//       addEntry(map, key_str, val);
//     } else {
//       // entry is a constant, store it
//       const auto &[txt_key, val_str] = *txt_entry;
//       const auto &key_str = txtKeyVals.at(txt_key);

//       addEntry(map, key_str, val_str);
//     }
//   }

//   return map;
// }

// string Service::calcTxtVal(Key key) const {
//   const auto key_str = txtKeyVals.at(key);

//   switch (key) {
//     case PublicKey:
//       return _base.pk;

//     default:
//       fmt::print("key={} unhandled\n", key_str);
//       break;
//   }

//   return string("unknown");
// }

// const string Service::keyString(TxtKey key) const { return txtKeyVals.at(key); }

// const string Service::keyValue(TxtKey key) const {
//   auto calc = txtVals.end();
//   auto txt_entry = txtVals.find(key);

//   // value is a constant
//   if (txt_entry != calc) {
//     const auto val = txt_entry->second;

//     return val;
//   }

//   // value must be calculated
//   const auto val = calcTxtVal(key);

//   return val;
// }

// const ServiceAndReg Service::nameAndReg(Type type) const {
//   return _service_vals.at(atype);

//   if (type == AirPlayTCP) {
//     // _airplay._tcp the service name is just the service base
//     return make_tuple(string(_base.service), string("_airplay._tcp"));
//   }

//   if (type == RaopTCP) {
//     // _raop._tcp service name is:
//     //   hw_addr bytes (no separator between bytes), followed by '@'
//     //   then service base
//     ServiceName name = fmt::format("{}@{}", _base.host->deviceId(), _base.service);

//     return make_tuple(name, string("_raop._tcp"));
//   }

//   throw(runtime_error("bad service type"));
// }

// const char *Service::plKey(Key key) const { return keyString(key).c_str(); }

// plist_t Service::plVal(Key key) const { return plist_new_string(keyValue(key).c_str());
// }

} // namespace core
} // namespace pierre