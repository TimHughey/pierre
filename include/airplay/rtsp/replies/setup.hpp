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

#include "aplist/aplist.hpp"
#include "rtsp/reply.hpp"

namespace pierre {
namespace rtsp {

class Setup {
public:
  Setup(Request &request, Reply &reply, std::shared_ptr<Ctx> ctx) noexcept;

private:
  bool has_streams() noexcept;
  bool no_streams() noexcept;

private:
  // order dependent
  Request &request;
  Reply &reply;
  std::shared_ptr<Ctx> ctx;
  Aplist rdict; // the request dict

  // order independent
  Aplist reply_dict; // entire reply dict

public:
  static constexpr csv module_id{"rtsp::SETUP"};
};

} // namespace rtsp
} // namespace pierre