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

#include "base/uint8v.hpp"
#include "base/typical.hpp"

#include <ranges>
#include <string_view>

namespace ranges = std::ranges;
using namespace std::literals::string_view_literals;

namespace pierre {

void uint8v::dump() const {
  __LOG0(LCOL01 " size={}\n{}{}\n", moduleId(), //
         csv("<unspecified>"), size(), __LOG_COL2, inspect());
}

string uint8v::inspect() const { // virtual
  string msg;

  if (printable()) {
    return string(view());
  }

  return toByteArrayString(msg);
}

string &uint8v::toByteArrayString(string &msg) const {
  const auto filler = fmt::format(LCOL01, LBLANK, LBLANK);

  ranges::for_each(*this, [&, w = std::back_inserter(msg), num = 1](const auto byte) {
    if (auto last_nl = msg.find_last_of('\n'); (last_nl != string::npos) && (last_nl > 100)) {
      fmt::format_to(w, "\n{} ", filler);

    } else {
      fmt::format_to(w, "{:03}[0x{:02x}] ", num, byte);
    }
  });

  if (msg.back() != '\n') {
    msg.push_back('\n');
  }

  return msg;
}

} // namespace pierre