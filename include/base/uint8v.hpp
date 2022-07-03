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

#pragma once

#include "base/types.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ranges>
#include <vector>

namespace {
namespace ranges = std::ranges;
}

namespace pierre {

class uint8v : public std::vector<uint8_t> {
public:
  template <typename T> const T *raw() const { return (const T *)data(); }
  const string_view view() const { return string_view(raw<char>(), size()); }

  virtual void dump(csv type = csv("unknown")) const;
  virtual string inspect() const;

protected:
  bool printable() const {
    if (size()) {
      return ranges::all_of( // only look at the first 10%
          begin(), begin() + (size() / 10),
          [](auto c) { return std::isprint(static_cast<unsigned char>(c)); });
    }

    return false;
  }
  string &toByteArrayString(string &msg) const;
};

} // namespace pierre