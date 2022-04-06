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

#include <source_location>

#include "fmt/format.h"
#include "rtsp/reply/factory.hpp"
#include "rtsp/reply/fairplay.hpp"
#include "rtsp/reply/options.hpp"

namespace pierre {
namespace rtsp {

ReplyShared Factory::create(RequestShared request) {

  auto method = request->method();
  auto path = request->path();

  // NOTE: compare sequence equivalent to AirPlay conversation

  if (method.compare("POST") == 0) {
    if (path.compare("/fp-setup") == 0) {
      return std::make_shared<FairPlay>(request);
    }
  }

  if (method.compare("OPTIONS") == 0) {
    if (path.compare("*") == 0) {
      return std::make_shared<Options>(request);
    }
  }

  request->dump(Request::DumpKind::RawOnly);

  const auto loc = std::source_location::current();
  fmt::print("In file {} at line {}\n", loc.file_name(), loc.line());
  fmt::print("\tunhandled method={} path={}\n\n", method, path);

  throw(std::runtime_error("unhandled method/path"));
}

} // namespace rtsp

} // namespace pierre
