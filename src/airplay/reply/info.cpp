/*
    Pierre - Custom Light Show for Wiss Landing
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
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#include "reply/info.hpp"

#include <fmt/format.h>
#include <iterator>
#include <memory>

namespace pierre {
namespace airplay {
namespace reply {

namespace heaeder = pierre::packet::header;

bool Info::populate() {
  rdict = plist();

  // if dictionary is empty this is a stage 2 packet
  if (rdict.empty()) {
    _stage = Stage2;
    return stage2();
  }

  // not stage 2, is this a stage 1 requset?
  constexpr auto qual_key = "qualifier";
  constexpr auto qual_val = "txtAirPlay";

  // confirm dict contains: qualifier -> array[0] == txtAirPlay
  if (rdict.compareStringViaPath(qual_val, 2, qual_key, 0)) {
    _stage = Stage1;
    return stage1();
  }

  // if we've reached this point this is an app error
  responseCode(packet::RespCode::BadRequest);
  return false;
}

bool Info::stage1() {
  using enum service::Type;
  using enum service::Key;

  // create the reply dict from an embedded plist
  packet::Aplist reply_dict(packet::Aplist::GetInfoRespStage1);

  // create the qualifier data value
  auto qual_data = fmt::memory_buffer();
  auto here = back_inserter(qual_data);
  auto digest = service().keyValList(AirPlayTCP);

  // an entry in the plist is an concatenated list of txt values
  // published via mDNS
  for (const auto &entry : *digest) {
    const auto [key_str, val_str] = entry;
    fmt::format_to(here, "{}={}", key_str, val_str);
  }

  constexpr auto qual_key = "qualifier";
  reply_dict.setData(qual_key, qual_data);

  // add specific dictionary entries
  // features
  const auto features_key = service().fetchKey(apFeatures);
  const auto features_val = service().features();
  reply_dict.setUint(nullptr, features_key, features_val);

  // system flags
  const auto system_flags_key = service().fetchKey(apSystemFlags);
  const auto system_flags_val = service().systemFlags();
  reply_dict.setUint(nullptr, system_flags_key, system_flags_val);

  // string vals
  auto want_keys = std::array{apDeviceID, apAirPlayPairingIdentity, ServiceName, apModel};

  for (const auto key : want_keys) {
    const auto [key_str, val_str] = service().fetch(key);
    reply_dict.setStringVal(nullptr, key_str, val_str);
  }

  size_t bytes = 0;
  auto binary = reply_dict.toBinary(bytes);

  copyToContent(binary, bytes);

  responseCode(OK);
  headers.add(header::type::ContentType, header::val::AppleBinPlist);

  return true;
}

bool Info::stage2() {
  using KeySeq = service::KeySeq;
  using enum service::Key;

  // the response dict is based on the request
  packet::Aplist reply_dict(packet::Aplist::GetInfoRespStage1);

  // handle the uints first
  const auto features_key = service().fetchKey(apFeatures);
  const auto features_val = service().features();
  reply_dict.setUint(nullptr, features_key, features_val);

  const auto status_flags_key = service().fetchKey(apStatusFlags);
  const auto status_flags_val = service().systemFlags();
  reply_dict.setUint(nullptr, status_flags_key, status_flags_val);

  // get the key/vals of interest
  const auto want_kv =
      KeySeq{apDeviceID, apAirPlayPairingIdentity, ServiceName, apModel, PublicKey};

  auto kv_list = service().keyValList(want_kv); // returns shared_ptr

  // must deference kv_list (shared_ptr) to get to the actual kv_list
  for (const auto &entry : *kv_list) {
    const auto &[key_str, val_str] = entry;

    reply_dict.setStringVal(nullptr, key_str, val_str);
  }

  size_t bytes = 0;
  auto binary = reply_dict.toBinary(bytes);

  copyToContent(binary, bytes);

  responseCode(OK);
  headers.add(header::type::ContentType, header::val::AppleBinPlist);

  return true;
}

} // namespace reply
} // namespace airplay
} // namespace pierre
