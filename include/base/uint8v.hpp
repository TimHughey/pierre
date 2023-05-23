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
#include <concepts>
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

/// @brief Multi-purpose byte container
class uint8v : public std::vector<uint8_t> {

public:
  // a vector of pairs where each pair is the start pos and end pos of the
  // found delims
  using delims_t = std::vector<std::pair<ssize_t, ssize_t>>;

public:
  /// @brief Construct an empty container
  uint8v() = default;

  /// @brief Construct container with count bytes reserved
  /// @param count bytes to reserve
  uint8v(std::ptrdiff_t count) noexcept : std::vector<uint8_t>(count) {}

  /// @brief Constructor container with count bytes allocated
  /// @param count byte to allocate
  /// @param byte fill with value (default 0x00)
  uint8v(auto count, uint8_t byte = 0x00) noexcept : std::vector<uint8_t>(count, byte) {}

  /// @brief Assign the span to the container
  /// @param span Span representing uint8_t
  void assign_span(const std::span<uint8_t> &span) noexcept { assign(span.begin(), span.end()); }

  /// @brief Pointer to raw container data
  /// @tparam T Pointer type
  /// @param offset Offset applied to pointer, default 0
  /// @return Raw pointer of type T with offset applied
  template <typename T = char> T *data_as(std::ptrdiff_t offset = 0) noexcept {
    return (T *)(data() + offset);
  }

  /// @brief Find delimiters
  /// @param delims_want container of delimiters to find
  /// @return Vector of pairs consisting of the start pos and length
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

  /// @brief Return an iterator into the container with an offest
  /// @param bytes bytes to offset
  /// @return Iterator offset by bytes
  uint8v::iterator from_begin(std::ptrdiff_t bytes) { return begin() + bytes; }

  /// @brief Return an iterator into the container from the end offset by bytes
  /// @param bytes bytes to offset
  /// @return Iterator offset by bytes
  uint8v::iterator from_end(std::ptrdiff_t bytes) { return end() - bytes; }

  /// @brief Return const raw pointer to container data with offset applied
  /// @tparam T Treat as data type
  /// @param offset bytes to offset, default 0
  /// @return Raw const pointer to container data with offset applied
  template <typename T = char> const T *raw(size_t offset = 0) const noexcept {
    return (const T *)(data() + (sizeof(T) * offset));
  }

  /// @brief Raw pointer to container data with offset
  /// @tparam T Type to use for returned pointer
  /// @tparam C sizeof to use while calculating offset
  /// @param offset offset as sizeof C
  /// @return Raw pointer to container data with offset applied
  template <typename T, typename C = size_t> T *raw_buffer(C offset = 0U) const noexcept {

    if constexpr (std::same_as<C, std::ptrdiff_t>) {
      return (T *)(data() + offset);
    } else if constexpr (std::same_as<C, size_t>) {
      return (T *)(data() + (sizeof(T) * offset));
    } else {
      static_assert(AlwaysFalse<T>, "unsupported offset");
      return (T *)data();
    }
  }

  /// @brief Size of container with specific return type
  /// @tparam T Return type, signed or unsigned
  /// @return size of container as specified type
  template <typename T> const T size1() const noexcept {
    if constexpr (std::signed_integral<T>) {
      return std::ssize(*this);
    } else if constexpr (std::unsigned_integral<T>) {
      return std::size(*this);
    } else {
      static_assert(AlwaysFalse<T>, "unsupported size type");
      return size();
    }
  }

  /// @brief Convert bytes at offset to an uint32_t
  /// @param offset offset into container
  /// @param n number of bytes to use in conversion
  /// @return converted uint32_t
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

  /// @brief Return const string view starting at offset for bytes
  /// @param offset starting offset, default to 0
  /// @param bytes count of bytes to include, default to 0
  /// @return
  csv view(const size_t offset = 0, size_t bytes = 0) const noexcept {
    bytes = (bytes == 0) ? size() : bytes;
    return string_view(raw<char>(offset), bytes);
  }

protected:
  /// @brief Is container printable
  /// @return boolean
  bool printable() const noexcept {
    if (size()) {
      return ranges::all_of( // only look at the first 10%
          begin(), begin() + (size() / 10),
          [](auto c) { return std::isprint(static_cast<unsigned char>(c)); });
    }

    return false;
  }

public:
  static constexpr auto module_id_base{"uint8v"sv};
};

} // namespace pierre