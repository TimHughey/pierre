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
#include "airplay/headers.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"

namespace pierre {
namespace rtsp {

class Request {
public:
  Request() = default;

  void clear() noexcept {
    content = Content();
    headers = Headers();
  }

  void save(const uint8v &wire) noexcept;

public:
  Content content;
  Headers headers;

  static constexpr csv module_id{"rtsp::REQUEST"};
};

} // namespace rtsp
} // namespace pierre