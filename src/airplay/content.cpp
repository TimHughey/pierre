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

#include "content.hpp"
#include "base/logger.hpp"
#include "headers.hpp"

#include <iomanip>

namespace pierre {

void Content::dump() const {
  string msg;
  auto w = std::back_inserter(msg);

  fmt::format_to(w, "type={} bytes={} ", type, size());

  if (type == hdr_val::TextParameters) {
    fmt::format_to(w, "parameters='{}'", make_single_line());
  } else if (is_multi_line()) {
    fmt::format_to(w, "\n{}{}", INFO_TAB, inspect());
  } else if (size() == 0) {
    fmt::format_to(w, "CONTENT EMPTY");
  } else {
    fmt::format_to(w, "val='{}'", inspect()); // output inspect msg on the same line
  }

  INFO(moduleId(), "DUMP", "{}\n", msg);
}

} // namespace pierre
