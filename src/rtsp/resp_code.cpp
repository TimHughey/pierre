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

#include <string_view>
#include <unordered_map>

#include "rtsp/resp_code.hpp"

namespace pierre {
namespace rtsp {

typedef const std::unordered_map<RespCode, const char *> RespCodeMap;

using enum RespCode;

static RespCodeMap _resp_code_ccs{{OK, "OK"},
                                  {AuthRequired, "Connection Authorization Required"},
                                  {BadRequest, "Bad Request"},
                                  {InternalServerError, "Internal Server Error"},
                                  {Unauthorized, "Unauthorized"},
                                  {Unavailable, "Unavailable"},
                                  {NotImplemented, "Not Implemented"}};

std::string_view respCodeToView(RespCode resp_code) {
  return std::string_view(_resp_code_ccs.at(resp_code));
}

} // namespace rtsp
} // namespace pierre