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

#include "packet/content.hpp"

#include <cctype>
#include <fmt/format.h>
#include <string_view>

namespace pierre {
namespace packet {

const std::string_view Content::toStringView() const {
  ccs _data = (const char *)data();

  return std::string_view(_data, size());
}

void Content::dump() const {
  fmt::print("\nCONTENT DUMP type={} bytes={}\n", _type, size());

  if (size() > 0) {
    if (printable()) {
      fmt::print("{}\n", toStringView());
      return;
    }

    // not printable data, dump as bytes
    for (size_t idx = 0; idx < size();) {
      fmt::print("{:03}[0x{:02x}] ", idx, at(idx));

      if ((++idx % 10) == 0)
        fmt::print("\n");

      if (idx > 100) {
        break;
      }
    }

    fmt::print("\n");
  }
}

bool Content::printable() const {
  auto rc = true;

  for (size_t idx = 0; rc && (idx < size());) {
    const auto byte = at(0);

    if (byte != 0x00) {
      rc &= std::isprint(static_cast<unsigned char>(byte));
    }
  }

  return rc;
}

} // namespace packet
} // namespace pierre