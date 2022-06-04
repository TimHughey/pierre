
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
#include "packet/content.hpp"
#include "packet/headers.hpp"
#include "reply/reply.hpp"

#include <fmt/format.h>
#include <string_view>

namespace pierre {
namespace airplay {
namespace reply {

using namespace pierre::packet;

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
  const auto param = rContent().toStringView();

  if (param.starts_with("volume")) {
    auto buff = fmt::memory_buffer();

    // NOTE: special case -- the content must include the separators
    // so they are including in the content length
    fmt::format_to(buff, "\r\nvolume: {:.6}\r\n", -24.09);

    copyToContent(buff);
    headers.add(header::type::ContentType, header::val::TextParameters);
    responseCode(OK);

    rc = true;
  }

  return rc;
}

bool Parameter::handleSet() {
  // if (rHeaders().contentType() == csv(header::val::TextParameters)) {
  //   constexpr auto f = FMT_STRING("{} SET_PARAMETER content={}\n");
  //   fmt::print(f, runTicks(), rContent().toStringView());
  // }

  responseCode(RespCode::OK);

  return true;
}

} // namespace reply

} // namespace airplay
} // namespace pierre