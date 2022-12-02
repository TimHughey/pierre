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

#pragma once

#include "base/types.hpp"
#include "reply/reply.hpp"

namespace pierre {
namespace airplay {
namespace reply {

class Parameter : public Reply {
public:
  Parameter() : Reply("PARAMETER") {}

  bool populate() override {
    auto rc = true;

    const auto &rheaders = rHeaders();

    if (rheaders.method().starts_with("GET_PARAMETER")) {
      csv param = rContent().view();

      if (param.starts_with("volume")) {
        static csv full_volume("\r\nvolume: 0.0\r\n");

        copyToContent(full_volume);
        headers.add(hdr_type::ContentType, hdr_val::TextParameters);
      }

    } else if (rheaders.method().starts_with("SET_PARAMETER")) {
      if (rContent() == hdr_val::TextParameters) {
        // rContent().dump();
      }

    } else {
      rc = false;
    }

    resp_code(rc ? RespCode::OK : RespCode::BadRequest);

    return rc;
  }
};

} // namespace reply
} // namespace airplay
} // namespace pierre