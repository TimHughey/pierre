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

#include "aplist.hpp"
#include "base/conf/token.hpp"
#include "rtsp/headers.hpp"
#include "rtsp/reply.hpp"

namespace pierre {
namespace rtsp {

class Setup {
public:
  Setup(const uint8v &content_in, const Headers &headers_in, Reply &reply, Ctx *ctx) noexcept;

private:
  bool has_streams() noexcept;
  bool no_streams() noexcept;

private:
  // order dependent
  conf::token ctoken;
  Aplist rdict;              // the request dict
  const Headers &headers_in; /// request headers
  Reply &reply;
  Ctx *ctx;

  // order independent
  Aplist reply_dict; // entire reply dict

public:
  static constexpr csv module_id{"rtsp.reply.setup"};
};

} // namespace rtsp
} // namespace pierre