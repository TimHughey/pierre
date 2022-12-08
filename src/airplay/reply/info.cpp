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

#include "reply/info.hpp"
#include "aplist/aplist.hpp"
#include "base/logger.hpp"
#include "base/uint8v.hpp"
#include "config/config.hpp"
#include "mdns/service.hpp"
#include "reply/dict_keys.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

namespace pierre {
namespace airplay {
namespace reply {

// class static data
std::vector<char> Info::reply_xml;

/// @brief Initialize static data (reply pdict)
void Info::init() noexcept { // static
  static constexpr csv module_id{"reply::INFO"};
  static constexpr csv fn_id{"INIT"};
  namespace fs = std::filesystem;

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

bool Info::populate() {
  auto rc = true;
  auto service = di->rtsp_ctx->service;

  // an error with INFO is rare so let's build the common portions
  // of the reply then augment based on the stage we're handling

  // common list of keys to add to reply (only stage 2 adds a key)
  txt_opt_seq_t want_keys{txt_opt::apDeviceID, txt_opt::apAirPlayPairingIdentity,
                          txt_opt::ServiceName, txt_opt::apModel};

  // the plist from static data read by init()
  Aplist reply_dict(reply_xml);
  reply_dict.setUint(service->key_val<uint64_t>(txt_opt::apFeatures));

  if (rdict.empty()) { // if dictionary is empty this is a stage 2 message

    // INFO stage 2 appears to want apStatusFlags
    reply_dict.setUint(service->key_val<uint64_t>(txt_opt::apStatusFlags));

    // INFO stage 2 also includes the public key
    want_keys.emplace_back(txt_opt::PublicKey);

  } else if (rdict.compareStringViaPath(dv::TXT_AIRPLAY.data(), 2, dk::QUALIFIER.data(), 0)) {
    // INFO stage 1 request because it contains: qualifiers -> array[0] == txtAirPlay

    // in thre reply we concatenate all the AirPlayTCP mdns data into a data chunk
    reply_dict.setData("qualifier", service->make_txt_string(txt_type::AirPlayTCP));

    // INFO stage 2 appears to want apSystemFlags (slight nuance vs stage 1)
    reply_dict.setUint(service->key_val<uint64_t>(txt_opt::apSystemFlags));

  } else {
    rc = false;
  }

  if (rc) {

    // populate plist keys
    for (const auto &key : want_keys) {
      reply_dict.setStringVal(nullptr, service->key_val(key));
    }

    size_t bytes = 0;
    auto binary = reply_dict.toBinary(bytes);

    copyToContent(binary, bytes);

    headers.add(hdr_type::ContentType, hdr_val::AppleBinPlist);
  }

  resp_code(rc ? RespCode::OK : RespCode::BadRequest);

  return rc;
}

} // namespace reply
} // namespace airplay
} // namespace pierre
