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

#include "reply/reply.hpp"

namespace pierre {
namespace airplay {
namespace reply {

class Feedback : public Reply {
public:
  Feedback() : Reply("FEEDBACK") {}

  bool populate() override {
    //  Aplist stream0_dict;

    // stream0_dict.setUint(dk::TYPE, 103);
    // stream0_dict.setReal(dk::SR, 44100.0);

    // Aplist reply_dict;

    // reply_dict.setArray(dk::STREAMS, stream0_dict);

    // size_t bytes = 0;
    // auto binary = reply_dict.toBinary(bytes);
    // copyToContent(binary, bytes);

    // headers.add(hdr_type::ContentType, hdr_val::AppleBinPlist);

    di->rtsp_ctx->feedback_msg();

    resp_code(RespCode::OK);

    return true;
  }
};

} // namespace reply
} // namespace airplay
} // namespace pierre