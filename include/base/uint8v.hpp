/* Pierre - Custom Light Show for Wiss Landing
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

    https://www.wisslanding.co */

#pragma once

#include "base/types.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <ranges>
#include <vector>

namespace {
namespace ranges = std::ranges;
} // namespace

namespace pierre {

class uint8v : public std::vector<uint8_t> {
public:
  uint8v() = default;
  uint8v(size_t reserve_default) : reserve_default(reserve_default) { reserve(reserve_default); }
  uint8v(size_t count, uint8_t byte) : std::vector<uint8_t>(count, byte), reserve_default(count) {}

  bool multiLineString() const {
    return ranges::count_if(view(), [](const char c) { return c == '\n'; }) > 2;
  }

  uint8v::iterator from_begin(std::ptrdiff_t bytes) { return begin() + bytes; }
  uint8v::iterator from_end(std::ptrdiff_t bytes) { return end() - bytes; }

  template <typename T> const T *raw() const { return (const T *)data(); }
  void reset(size_t reserve_bytes = 0) {
    clear();

    if ((reserve_bytes == 0) && (reserve_default > 0)) {
      reserve(reserve_default);
    } else if (reserve_bytes > 0) {
      reserve(reserve_bytes);
    }
  }

  string toSingleLineString() const {
    auto vspace = [](const char c) { return ((c != '\n') && (c != '\r')); };
    auto v = ranges::filter_view(view(), vspace);

    return string(v.begin(), v.end());
  }

  uint32_t to_uint32(size_t offset, int n) const {
    uint32_t val = 0;
    size_t shift = (n - 1) * 8;

    for (auto it = std::counted_iterator{begin() + offset, n}; //
         it != std::default_sentinel;                          //
         ++it, shift -= 8)                                     //
    {
      val += *it << shift;
    }

    return val;
  }

  const string_view view() const { return string_view(raw<char>(), size()); }

  // debug, logging

  virtual void dump() const;
  virtual string inspect() const;
  virtual csv moduleId() const { return module_id_base; }

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

private:
  size_t reserve_default = 0;

  static constexpr csv module_id_base{"UINT8V"};
};

} // namespace pierre