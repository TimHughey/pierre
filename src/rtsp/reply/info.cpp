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
#include "rtsp/reply/info.hpp"
#include "rtsp/reply/xml.hpp"

using namespace std;

namespace pierre {
namespace rtsp {

constexpr auto qualifier = "txtAirPlay";
constexpr auto qualifier_len = strlen(qualifier);

bool Info::populate() {
  // need these to determine if stage1 or stage2
  const auto &content = requestContent();
  const auto plist_len = content.size();

  // if content is empty this is an info stage 2 packet
  if (plist_len == 0) {
    _stage = Stage2;
    return stage2();
  }

  // not stage 2, validate stage 1 packet

  constexpr auto qual_key = "qualifier";

  plist_t info_plist = nullptr;
  // remember, content buffer is uint8 so we must reinterpret_cast
  auto *data = reinterpret_cast<const char *>(content.data());

  // create the plist and wrap the pointer
  plist_from_memory(data, content.size(), &info_plist);
  auto __info_plist = make_unique<plist_t>(info_plist); // ensure mem freed

  // use access path to find qualifier and array element 0
  auto array_val0 = plist_access_path(info_plist, plist_len, qual_key, 0);

  // want to find this at array index 0

  // confirmation of stage 1 -- node is equal to the expected string
  if (plist_get_node_type(array_val0) == PLIST_STRING) {
    constexpr auto val = qualifier;
    constexpr auto len = qualifier_len;

    if (plist_string_val_compare_with_size(array_val0, val, len) == 0) {
      _stage = Stage1;

      return stage1();
    }
  }

  // if we've reached this point the packet is malformed
  return false;
}

bool Info::stage1() {
  using enum core::service::Type;
  using enum core::service::Key;

  const char *xml_raw = XML::doc(InfoStage1);
  const uint32_t xml_len = XML::len(InfoStage1);

  plist_t rplist = nullptr;
  plist_from_memory(xml_raw, xml_len, &rplist);
  auto __rplist = make_unique<plist_t>(rplist);

  // create the qualifier data value
  auto _qual_data = fmt::memory_buffer();
  auto here = back_inserter(_qual_data);
  auto digest = service()->keyValList(AirPlayTCP);

  // an entry in the plist is an concatenated list of txt values
  // published via mDNS
  for (const auto entry : *digest) {
    const auto [key_str, val_str] = entry;
    fmt::format_to(here, "{}={}", key_str, val_str);
  }

  auto qual_data = plist_new_data(_qual_data.data(), _qual_data.size());
  plist_dict_set_item(rplist, qualifier, qual_data);

  // add specific dictionary entries

  // first, handle the integers
  auto features_val = plist_new_uint(service()->features());
  auto features_key = service()->fetchKey(apFeatures);
  plist_dict_set_item(rplist, features_key, features_val);

  auto system_flags_pl = plist_new_uint(service()->systemFlags());
  auto system_flags_key = service()->fetchKey(apSystemFlags);
  plist_dict_set_item(rplist, system_flags_key, system_flags_pl);

  // second, handle the strings
  auto want_keys =
      array{apDeviceID, apAirPlayPairingIdentity, ServiceName, apModel};

  for (const auto key : want_keys) {
    const auto [key_str, val_str] = service()->fetch(key);

    auto pl_val = plist_new_string(val_str);
    plist_dict_set_item(rplist, key_str, pl_val);
  }

  // char *__buf = nullptr;
  // uint32_t __buf_len = 0;

  // plist_to_xml(rplist, &__buf, &__buf_len);

  // fmt::print("XML {}\n{}\n", fmt::ptr(rplist), __buf);

  char *pbin = nullptr;
  uint32_t pbin_len;

  plist_to_bin(rplist, &pbin, &pbin_len);
  auto __pbin = make_unique<char *>(pbin);

  // fmt::print("Info::stage1() plist_to_bin pbin={}, pbin_len={}\n",
  // fmt::ptr(pbin),
  //            pbin_len);

  const uint8_t *content = reinterpret_cast<const uint8_t *>(pbin);
  const size_t content_len = pbin_len;

  // fmt::print("Info::stage1() calling copyToContent() from={}, len={}\n",
  //            fmt::ptr(content), content_len);
  copyToContent(content, content_len);

  // fmt::print("/info stage1\n");
  // fmt::print("{}\n", fmt::to_string(qual_data));

  responseCode(OK);
  headerAdd(ContentType, AppleBinPlist);

  return true;
}

bool Info::stage2() {
  fmt::print("/info stage2\n");
  // plist_t response_plist = NULL;
  // plist_from_xml((const char *)plists_get_info_response_xml,
  // plists_get_info_response_xml_len,
  //                &response_plist);
  // plist_dict_set_item(response_plist, "features",
  // plist_new_uint(config.airplay_features));
  // plist_dict_set_item(response_plist, "statusFlags",
  //                     plist_new_uint(config.airplay_statusflags));
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

  return false;
}

} // namespace rtsp
} // namespace pierre
