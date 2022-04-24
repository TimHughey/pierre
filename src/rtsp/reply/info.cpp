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

#include <fmt/format.h>
#include <iterator>
#include <memory>
#include <plist/plist++.h>
#include <string>

#include "core/service.hpp"
#include "rtsp/aplist.hpp"
#include "rtsp/reply/info.hpp"

using namespace std;

namespace pierre {
namespace rtsp {

using enum Headers::Type2;
using enum Headers::Val2;

Info::Info(const Reply::Opts &opts) : Reply(opts), Aplist(rContent()) {
  // disable debug logging

  debugFlag(false);
}

bool Info::populate() {
  // if dictionary is empty this is a stage 2 packet
  if (dictEmpty()) {
    _stage = Stage2;
    return stage2();
  }

  // not stage 2, is this a stage 1 requset?
  constexpr auto qual_key = "qualifier";
  constexpr auto qual_val = "txtAirPlay";

  // confirm dict contains: qualifier -> array[0] == txtAirPlay
  if (dictCompareStringViaPath(qual_val, 2, qual_key, 0)) {
    _stage = Stage1;
    return stage1();
  }

  // if we've reached this point the packet is malformed
  return false;
}

bool Info::stage1() {
  using enum core::service::Type;
  using enum core::service::Key;

  // create the reply dict from an embedded plist
  Aplist reply_dict(GetInfoRespStage1);

  // create the qualifier data value
  auto qual_data = fmt::memory_buffer();
  auto here = back_inserter(qual_data);
  auto digest = service()->keyValList(AirPlayTCP);

  // an entry in the plist is an concatenated list of txt values
  // published via mDNS
  for (const auto &entry : *digest) {
    const auto [key_str, val_str] = entry;
    fmt::format_to(here, "{}={}", key_str, val_str);
  }

  constexpr auto qual_key = "qualifier";
  reply_dict.dictSetData(qual_key, qual_data);

  // add specific dictionary entries
  // features
  const auto features_key = service()->fetchKey(apFeatures);
  const auto features_val = service()->features();
  reply_dict.dictSetUint(nullptr, features_key, features_val);

  // system flags
  const auto system_flags_key = service()->fetchKey(apSystemFlags);
  const auto system_flags_val = service()->systemFlags();
  reply_dict.dictSetUint(nullptr, system_flags_key, system_flags_val);

  // string vals
  auto want_keys = array{apDeviceID, apAirPlayPairingIdentity, ServiceName, apModel};

  for (const auto key : want_keys) {
    const auto [key_str, val_str] = service()->fetch(key);
    reply_dict.dictSetStringVal(nullptr, key_str, val_str);
  }

  size_t bytes = 0;
  auto binary = reply_dict.dictBinary(bytes);

  copyToContent(binary, bytes);

  responseCode(OK);
  headers.add(ContentType, AppleBinPlist);

  return true;
}

bool Info::stage2() {
  using KeySeq = core::service::KeySeq;
  using enum core::service::Key;

  // the response dict is based on the request
  Aplist reply_dict(GetInfoRespStage1);

  // handle the uints first
  const auto features_key = service()->fetchKey(apFeatures);
  const auto features_val = service()->features();
  reply_dict.dictSetUint(nullptr, features_key, features_val);

  const auto status_flags_key = service()->fetchKey(apStatusFlags);
  const auto status_flags_val = service()->systemFlags();
  reply_dict.dictSetUint(nullptr, status_flags_key, status_flags_val);

  // get the key/vals of interest
  const auto want_kv =
      KeySeq{apDeviceID, apAirPlayPairingIdentity, ServiceName, apModel, PublicKey};

  auto kv_list = service()->keyValList(want_kv); // returns shared_ptr

  // must deference kv_list (shared_ptr) to get to the actual kv_list
  for (const auto &entry : *kv_list) {
    const auto &[key_str, val_str] = entry;

    reply_dict.dictSetStringVal(nullptr, key_str, val_str);
  }

  size_t bytes = 0;
  auto binary = reply_dict.dictBinary(bytes);

  copyToContent(binary, bytes);

  responseCode(OK);
  headers.add(ContentType, AppleBinPlist);

  // plist_dict_set_item(response_plist, "deviceID",
  // plist_new_string(config.airplay_device_id));
  // plist_dict_set_item(response_plist, "pi",
  // plist_new_string(config.airplay_pi)); plist_dict_set_item(response_plist,
  // "name", plist_new_string(config.service_name)); char *vs =
  // get_version_string();
  // // plist_dict_set_item(response_plist, "model", plist_new_string(vs));
  // plist_dict_set_item(response_plist, "model", plist_new_string("Shairport
  // Sync")); free(vs);
  // // pkString_make(pkString, sizeof(pkString), config.airplay_device_id);
  // // plist_dict_set_item(response_plist, "pk", plist_new_string(pkString));
  // plist_to_bin(response_plist, &resp->content, &resp->contentlength);
  // plist_free(response_plist);
  // msg_add_header(resp, "Content-Type", "application/x-apple-binary-plist");
  // debug_log_rtsp_message(3, "GET /info Stage 2 Response", resp);
  // resp->respcode = 200;
  // return;

  // plist_t response_plist = NULL;
  // plist_from_xml((const char *)plists_get_info_response_xml,
  // plists_get_info_response_xml_len,
  //                &response_plist);

  // plist_dict_set_item(response_plist, "features",
  // plist_new_uint(config.airplay_features));

  // plist_dict_set_item(response_plist, "statusFlags",
  //                     plist_new_uint(config.airplay_statusflags));

  return true;
}

} // namespace rtsp
} // namespace pierre
