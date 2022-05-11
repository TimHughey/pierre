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

#include "core/host.hpp"
#include "service/types.hpp"
#include "typedefs.hpp"

#include <cstdint>
#include <memory>
#include <plist/plist++.h>
#include <tuple>
#include <unordered_map>

namespace pierre {

class Service {
public:
  enum Flags : uint8_t { DeviceSupportsRelay = 11 };

public:
  using KeyVal = service::KeyVal;
  using sKeyValList = service::sKeyValList;
  using Type = service::Type;
  using Key = service::Key;
  using KeySeq = service::KeySeq;
  using enum service::Key;

private:
  struct Inject {
    Host &host;
  };

public:
  Service(const Inject &di);

  // general API
  // void adjustSystemFlags(Flags flag);
  void deviceSupportsRelay(bool on_off = true);
  auto features() const { return _features_val; }
  const KeyVal fetch(const Key key) const;
  ccs fetchKey(const Key key) const;
  ccs fetchVal(const Key key) const;
  sKeyValList keyValList(Type service_type) const;
  sKeyValList keyValList(const KeySeq &keys_want) const;
  const KeyVal nameAndReg(Type type) const;

  // primary port for AirPlay2 connections
  uint16_t basePort() { return _base_port; }

  // easy access to commonly needed values
  ccs deviceID() const { return fetchVal(apDeviceID); }
  ccs name() const { return fetchVal(ServiceName); }

  // system flags (these change based on AirPlay)
  uint64_t systemFlags() const { return _system_flags; }

private:
  void addFeatures();
  void addRegAndName();
  void addSystemFlags();

  void saveCalcVal(Key key, csr val);
  void saveCalcVal(Key key, ccs val);

private:
  static constexpr uint16_t _base_port = 7000;

  // Advertised with mDNS and returned with GET /info, see
  // https://openairplay.github.io/airplay-spec/status_flags.html
  // 0x4: Audio cable attached, no PIN required (transient pairing)
  // 0x204: Audio cable attached, OneTimePairingRequired
  // 0x604: Audio cable attached, OneTimePairingRequired, device setup for
  // Homekit access control
  const uint32_t _system_flags_initial = 0x04;
  uint32_t _system_flags = 0x04;

  // features code is 64-bits and is used for both mDNS and plist
  // for mDNS advertisement
  //  1. least significant 32-bits (with 0x prefix)
  //  2. comma seperator
  //  3. most significant 32-bits (with 0x prefix)
  //
  // examples:
  //  mDNS  -> 0x1C340405F4A00: features=0x405F4A00,0x1C340
  //  plist -> 0x1C340405F4A00: 496155702020608 (signed int)
  //  and in a signed decimal number in the plist:

  // uint64_t _features_val = 0x1C340445F8A00; // based on Sonos Amp
  uint64_t _features_val = 0x00;
  std::string _features_mdns;
  std::string _features_plist;

  static service::KeyValMap _kvm;
  static service::KeyValMapCalc _kvm_calc;
  static service::KeySequences _key_sequences;
};
} // namespace pierre