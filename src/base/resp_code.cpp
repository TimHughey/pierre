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

#include "resp_code.hpp"

#include <map>

namespace pierre {

typedef const std::map<RespCode, const char *> RespCodeMap;

static RespCodeMap _resp_code_ccs{{RespCode::OK, "OK"},
                                  {RespCode::AuthRequired, "Connection Authorization Required"},
                                  {RespCode::BadRequest, "Bad Request"},
                                  {RespCode::InternalServerError, "Internal Server Error"},
                                  {RespCode::Unauthorized, "Unauthorized"},
                                  {RespCode::Unavailable, "Unavailable"},
                                  {RespCode::NotImplemented, "Not Implemented"},
                                  {RespCode::Continue, "Continue"}};

csv respCodeToView(RespCode resp_code) { return csv(_resp_code_ccs.at(resp_code)); }

} // namespace pierre
