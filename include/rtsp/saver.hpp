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

#include "base/conf/token.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "headers.hpp"
#include "rtsp/aplist.hpp"
#include "rtsp/resp_code.hpp"

namespace pierre {
namespace rtsp {

class Saver {

private:
  static constexpr csv separator{"\r\n"};

public:
  enum Direction { IN, OUT };

public:
  Saver(Saver::Direction direction, const Headers &headers, const uint8v &content,
        const RespCode resp_code = RespCode(RespCode::code_val::OK)) noexcept;

private:
  // order dependent
  conf::token tokc;

  // order independent
  const string file;
  const string format;

public:
  string msg;
  static constexpr csv module_id{"rtsp.saver"};
};

} // namespace rtsp

} // namespace pierre
