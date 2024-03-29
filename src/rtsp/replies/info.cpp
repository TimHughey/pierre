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

#include "replies/info.hpp"
#include "aplist.hpp"
#include "base/uint8v.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"
#include "mdns/mdns.hpp"
#include "replies/dict_kv.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

namespace pierre {
namespace rtsp {

// class static data
std::vector<char> Info::reply_xml;

Info::Info(Reply &reply) noexcept {

  // notes:
  //  1. other open source implementations look for and build a stage 1
  //     reply when the request plist contains: qualifiers[0] = "txtAirPlay"
  //  2. comments from those implementations state a root level key of
  //     "qualifier" should contain a concatentated list of the txt values
  //     published as part of the AirPlayTCP zeroconf service
  //  3. this implementation has determined that a stage 1 reply is not
  //     required
  //  4. rather, the stage 2 reply consisting of following plist is sufficient

  // the overall reply dict is rather large so we load it from a file to save the
  // code required to build programmatically
  if (reply_xml.empty()) init(); // ensure the base reply XML is loaded
  Aplist reply_dict(reply_xml);

  Service &service = mDNS::service();

  // first, we add the uint64 values to the dict
  reply_dict.setUint(service.key_val<uint64_t>(txt_opt::apFeatures));
  reply_dict.setUint(service.key_val<uint64_t>(txt_opt::apStatusFlags));

  // now add the text values to the dict
  txt_opt_seq_t txt_keys{txt_opt::apDeviceID, txt_opt::apAirPlayPairingIdentity,
                         txt_opt::ServiceName, txt_opt::apModel, txt_opt::PublicKey};

  for (const auto &key : txt_keys) {
    reply_dict.setStringVal(nullptr, service.key_val(key));
  }

  // finally, convert the plist dictionary to binary and store as
  // content for inclusion in reply
  reply_dict.format_to(reply.content_out);
  reply.headers_out.add(hdr_type::ContentType, hdr_val::AppleBinPlist);

  reply(RespCode::OK);
}

/// @brief Initialize static data (reply pdict)
void Info::init() noexcept { // static
  static constexpr csv fn_id{"init"};

  csv sub_path{"../share/plist/get_info_resp.plist"};
  auto plist_path = config()->fs_exec_path().append(sub_path);

  if (std::ifstream is{plist_path, std::ios::binary | std::ios::ate}) {
    reply_xml.assign(is.tellg(), 0x00);

    is.seekg(0);
    if (is.read(reply_xml.data(), reply_xml.size())) {
      INFO(module_id, "debug", "{} size={}\n", plist_path.c_str(), reply_xml.size());
    }
  }

  if (reply_xml.empty()) {
    INFO(module_id, fn_id, "failed to load: {}\n", plist_path.c_str());
  }
}

} // namespace rtsp
} // namespace pierre
