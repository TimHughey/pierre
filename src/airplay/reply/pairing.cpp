/*
    Pierre - Custom Light Show for Wiss Landing
    Copyright (C) 2022  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#include "reply/pairing.hpp"
#include "aes/aes_ctx.hpp"

namespace pierre {
namespace airplay {
namespace reply {

bool Pairing::populate() {
  AesResult aes_result;

  if (path().starts_with("/pair-setup")) {
    aes_result = aesCtx().setup(rContent(), _content);
  }

  if (path().starts_with("/pair-verify")) {
    aes_result = aesCtx().verify(rContent(), _content);
  }

  if (_content.empty() == false) {
    headers.add(header::type::ContentType, header::val::OctetStream);
  }

  responseCode(aes_result.resp_code);

  return aes_result.ok;
}

} // namespace reply
} // namespace airplay
} // namespace pierre
