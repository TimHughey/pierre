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

#include <fmt/format.h>
#include <map>

namespace pierre {

std::map<RespCode::code_val, string> RespCode::txt_map{
    // comments for ease of IDE sorting
    {AuthRequired, "Connection Authorization Required"},
    {BadRequest, "Bad Request"},
    {Continue, "Continue"},
    {InternalServerError, "Internal Server Error"},
    {NotImplemented, "Not Implemented"},
    {OK, "OK"},
    {Unauthorized, "Unauthorized"},
    {Unavailable, "Unavailable"},
    // extra comma for ease of IDE sorting
};

const string RespCode::operator()() const noexcept {
  return fmt::format("{:d} {}", val, txt_map.at(val));
}

} // namespace pierre
