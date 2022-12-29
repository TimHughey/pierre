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

#pragma once

#include "base/types.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <ranges>
#include <span>
#include <tuple>
#include <vector>

namespace {
namespace ranges = std::ranges;
} // namespace

namespace pierre {

class uint8v : public std::vector<uint8_t> {

public:
  // a vector of pairs where each pair is the start pos and end pos of the
  // found delims
  using delims_t = std::vector<std::pair<ssize_t, ssize_t>>;

public:
  uint8v() = default;
  uint8v(auto count) noexcept { reserve(count); }
  uint8v(auto count, uint8_t byte) noexcept : std::vector<uint8_t>(count, byte) {}

  void assign_span(const std::span<uint8_t> &span) noexcept { assign(span.begin(), span.end()); }

  template <typename T = char> T *data_as(std::ptrdiff_t offset = 0) noexcept {
    return (T *)(data() + offset);
  }

  // find delimiters
  //
  // returns: vector of pairs of:
  //   a. position delim found
  //   b. length of delim
  auto find_delims(const auto &delims_want) noexcept {
    uint8v::delims_t delims;

    const auto search_view = view();

    for (ssize_t search_pos = 0; csv it : delims_want) {

      auto pos = search_view.find(it, search_pos);

      if (pos != search_view.npos) {
        delims.emplace_back(std::make_pair(pos, std::ssize(it)));

        search_pos += pos + std::ssize(it);

        if (search_pos >= std::ssize(search_view)) break;
      }
    }

    return delims;
  }

  // bool is_multi_line() const noexcept {
  //   return ranges::count_if(view(), [](const char c) { return c == '\n'; }) > 2;
  // }

  uint8v::iterator from_begin(std::ptrdiff_t bytes) { return begin() + bytes; }
  uint8v::iterator from_end(std::ptrdiff_t bytes) { return end() - bytes; }

  template <typename T = char> const T *raw(size_t offset = 0) const noexcept {
    return (const T *)(data() + (sizeof(T) * offset));
  }

  uint32_t to_uint32(size_t offset, int n) const noexcept {
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

  csv view(const size_t offset = 0, size_t bytes = 0) const noexcept {
    bytes = (bytes == 0) ? size() : bytes;
    return string_view(raw<char>(offset), bytes);
  }

  // debug, logging
  virtual csv moduleId() const { return module_id_base; }

protected:
  bool printable() const noexcept {
    if (size()) {
      return ranges::all_of( // only look at the first 10%
          begin(), begin() + (size() / 10),
          [](auto c) { return std::isprint(static_cast<unsigned char>(c)); });
    }

    return false;
  }

public:
  static constexpr csv module_id_base{"UINT8V"};
};

} // namespace pierre