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

#include "airplay/content.hpp"
#include "aplist/aplist.hpp"
#include "rtsp/reply.hpp"
#include "rtsp/request.hpp"

#include <vector>

namespace pierre {
namespace rtsp {

class Command {
public:
  Command(Request &request, Reply &reply) {
    Aplist rdict = request.content;

    if (!rdict.empty() && rdict.compareString("type", "updateMRSupportedCommands")) {
      // we don't support updateMRSupportedCommands
      reply(RespCode::BadRequest);
    } else {
      reply(RespCode::OK);
    }
  }
};

} // namespace rtsp
} // namespace pierre
