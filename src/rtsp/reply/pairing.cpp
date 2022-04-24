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

#include <exception>
#include <fmt/format.h>
#include <iterator>
#include <memory>
#include <string>

#include "rtsp/aes_ctx.hpp"
#include "rtsp/reply/pairing.hpp"

using namespace std;

namespace pierre {
namespace rtsp {

bool Pairing::populate() {
  AesResult aes_result;

  if (path().starts_with("/pair-setup")) {
    aes_result = aesCtx()->setup(rContent(), _content);
  }

  if (path().starts_with("/pair-verify")) {
    aes_result = aesCtx()->verify(rContent(), _content);
  }

  responseCode(aes_result.resp_code);

  if (_content.empty() == false) {
    headers.add(Headers::Type2::ContentType, Headers::Val2::OctetStream);
  }

  return aes_result.ok;
}

} // namespace rtsp
} // namespace pierre
