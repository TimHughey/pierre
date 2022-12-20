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

#include "rtsp/replies/info.hpp"
#include "aplist/aplist.hpp"
#include "base/logger.hpp"
#include "base/uint8v.hpp"
#include "config/config.hpp"
#include "mdns/service.hpp"
#include "rtsp/ctx.hpp"
#include "rtsp/replies/dict_kv.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

namespace pierre {
namespace rtsp {

// class static data
std::vector<char> Info::reply_xml;

Info::Info([[maybe_unused]] Request &request, Reply &reply, std::shared_ptr<Ctx> ctx) noexcept {

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

  auto service = ctx->service; // avoid multiple shared_ptr dereferences

  // first, we add the uint64 values to the dict
  reply_dict.setUint(service->key_val<uint64_t>(txt_opt::apFeatures));
  reply_dict.setUint(service->key_val<uint64_t>(txt_opt::apStatusFlags));

  // now add the text values to the dict
  txt_opt_seq_t txt_keys{txt_opt::apDeviceID, txt_opt::apAirPlayPairingIdentity,
                         txt_opt::ServiceName, txt_opt::apModel, txt_opt::PublicKey};

  for (const auto &key : txt_keys) {
    reply_dict.setStringVal(nullptr, service->key_val(key));
  }

  // finally, convert the plist dictionary to binary and store as
  // content for inclusion in reply

  size_t bytes = 0;
  auto binary = reply_dict.toBinary(bytes);
  reply.copy_to_content(binary.get(), bytes);

  reply.headers.add(hdr_type::ContentType, hdr_val::AppleBinPlist);

  reply.set_resp_code(RespCode::OK);
}

/// @brief Initialize static data (reply pdict)
void Info::init() noexcept { // static
  static constexpr csv module_id{"reply::INFO"};
  static constexpr csv fn_id{"INIT"};

  auto file_path = Config().fs_parent_path();
  file_path /= "../share/plist/get_info_resp.plist"sv;

  if (std::ifstream is{file_path, std::ios::binary | std::ios::ate}) {
    reply_xml.assign(is.tellg(), 0x00);

    is.seekg(0);
    if (is.read(reply_xml.data(), reply_xml.size())) {
      INFO(module_id, fn_id, "{} size={}\n", file_path.c_str(), reply_xml.size());
    }
  }

  if (reply_xml.empty()) {
    INFO(module_id, fn_id, "failed to load: {}\n", file_path.c_str());
  }
}

} // namespace rtsp
} // namespace pierre
