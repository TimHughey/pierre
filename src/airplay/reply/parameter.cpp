
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

#include "reply/parameter.hpp"
#include "base/content.hpp"
#include "base/typical.hpp"
#include "packet/headers.hpp"
#include "reply/reply.hpp"

#include <fmt/format.h>
#include <iterator>
#include <string_view>
#include <vector>

namespace pierre {
namespace airplay {
namespace reply {

bool Parameter::populate() {
  auto rc = false;

  if (method().starts_with("GET_PARAMETER")) {
    rc = handleGet();
  }

  if (method().starts_with("SET_PARAMETER")) {
    rc = handleSet();
  }

  return rc;
}

bool Parameter::handleGet() {
  auto rc = false;
  csv param = rContent().view();

  if (param.starts_with("volume")) {
    static csv full_volume("\r\nvolume: 0.0\r\n");

    copyToContent(full_volume);
    headers.add(header::type::ContentType, header::val::TextParameters);
    responseCode(OK);

    rc = true;
  }

  return rc;
}

bool Parameter::handleSet() {
  if (rHeaders().contentType() == csv(header::val::TextParameters)) {
    rContent().dump();
  }

  responseCode(RespCode::OK);

  return true;
}

} // namespace reply

} // namespace airplay
} // namespace pierre