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
#include <memory>
#include <plist/plist++.h>
#include <string>
#include <tuple>
#include <unordered_map>

#include "core/host.hpp"
#include "service/types.hpp"

namespace pierre {
namespace core {

using namespace std;

using namespace service;
using enum service::Key;
using enum service::Type;

// forward decl for shared_ptr typedef
class Service;
typedef std::shared_ptr<Service> sService;

class Service : public std::enable_shared_from_this<Service> {
public:
  [[nodiscard]] static sService create(sHost host) {
    // Not using std::make_shared<Best> because the c'tor is private.

    return sService(new Service(host));
  }

  sService getPtr() { return shared_from_this(); }

  // general API

  auto features() const { return _features_val; }
  const KeyVal fetch(const Key key) const;
  const char *fetchKey(const Key key) const;
  const char *fetchVal(const Key key) const;
  sKeyValList keyValList(Type service_type) const;
  // const ServiceAndReg nameAndReg(Type type) const;
  // const char *plKey(Key key) const;
  // plist_t plVal(Key key) const;
  const KeyVal nameAndReg(Type type) const;
  auto systemFlags() const { return _system_flags; }

private:
  Service(sHost host);

  void addFeatures();

  void addPublicKey(auto host);
  void addRegAndName(auto host);
  void addSystemFlags();

  void saveCalcVal(Key key, const string &val);

private:
  // Advertised with mDNS and returned with GET /info, see
  // https://openairplay.github.io/airplay-spec/status_flags.html
  // 0x4: Audio cable attached, no PIN required (transient pairing)
  // 0x204: Audio cable attached, OneTimePairingRequired
  // 0x604: Audio cable attached, OneTimePairingRequired, device setup for
  // Homekit access control
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
  string _features_mdns;
  string _features_plist;

  static service::KeyValMap _kvm;
  static service::KeyValMapCalc _kvm_calc;
  static service::KeySequences _key_sequences;
};
} // namespace core
} // namespace pierre