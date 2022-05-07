
//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2022  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#include <fmt/format.h>
#include <string_view>

#include "packet/aplist.hpp"
#include "rtsp/reply.hpp"
#include "rtsp/reply/feedback.hpp"

namespace pierre {
namespace rtsp {

using namespace packet;

Feedback::Feedback(const Reply::Opts &opts) : Reply(opts) {
  debugFlag(false);
  // maybe more
}

bool Feedback::populate() {
  // build the reply (includes portS for started services)
  Aplist::ArrayDicts array_dicts;
  auto &stream0_dict = array_dicts.emplace_back(packet::Aplist());

  stream0_dict.dictSetUint(nullptr, "type", 103);
  stream0_dict.dictSetReal("sr", 44100.0);

  Aplist reply_dict;

  reply_dict.dictSetArray("streams", array_dicts);

  size_t bytes = 0;
  auto binary = reply_dict.dictBinary(bytes);
  copyToContent(binary, bytes);

  headers.add(Headers::Type2::ContentType, Headers::Val2::AppleBinPlist);

  //
  // NOT FINISHED YET
  // plist_t payload_plist = plist_new_dict();
  // plist_dict_set_item(payload_plist, "type", plist_new_uint(103));
  // plist_dict_set_item(payload_plist, "sr", plist_new_real(44100.0));

  // plist_t array_plist = plist_new_array();
  // plist_array_append_item(array_plist, payload_plist);

  // plist_t response_plist = plist_new_dict();
  // plist_dict_set_item(response_plist, "streams",array_plist);

  // plist_to_bin(response_plist, &resp->content, &resp->contentlength);
  // plist_free(response_plist);
  // // plist_free(array_plist);
  // // plist_free(payload_plist);

  // msg_add_header(resp, "Content-Type", "application/x-apple-binary-plist");
  // debug_log_rtsp_message(2, "FEEDBACK response:", resp);

  responseCode(RespCode::OK);
  return true;
}

} // namespace rtsp
} // namespace pierre