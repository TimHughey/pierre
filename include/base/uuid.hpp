// Pierre
// Copyright (C) 2022 Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#pragma once

#include "base/types.hpp"

#include <compare>
#include <concepts>
#include <filesystem>
#include <fmt/format.h>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <uuid/uuid.h>
#include <vector>

namespace pierre {

struct UUID {
  friend struct fmt::formatter<UUID>;

  /// @brief Construct a UUID
  UUID() noexcept : storage(37, 0x00) {
    uuid_t binuuid;
    uuid_generate_random(binuuid);
    uuid_unparse_lower(binuuid, storage.data());
  }

  /// @brief Return object as string
  /// @return string
  const string &operator()() const noexcept { return storage; }

  /// @brief Compare to a string representation
  /// @param rhs string
  /// @return Varies based on compare
  auto operator<=>(const string &rhs) const { return storage <=> rhs; }

  /// @brief Compare to another UUID object
  /// @param rhs Object to compare
  /// @return Varies based on compare
  auto operator<=>(const UUID &rhs) const = default;

  /// @brief Convert UUID to string
  operator string() const noexcept { return storage; }

private:
  std::string storage;
};

} // namespace pierre

template <> struct fmt::formatter<pierre::UUID> {

  // Presentation format: 'f' - full, 's' - short (default)
  char presentation = 0x00;

  // Parses format specifications of the form ['f' | 's'].
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    // [ctx.begin(), ctx.end()) is a character range that contains a part of
    // the format string starting from the format specifications to be parsed,
    // e.g. in
    //
    //   fmt::format("{:f} - point of interest", point{1, 2});
    //
    // the range will contain "f} - point of interest". The formatter should
    // parse specifiers until '}' or the end of the range. In this example
    // the formatter should parse the 'f' specifier and return an iterator
    // pointing to '}'.

    // Please also note that this character range may be empty, in case of
    // the "{}" format string, so therefore you should check ctx.begin()
    // for equality with ctx.end().

    // Parse the presentation format and store it in the formatter:

    auto it = ctx.begin(), end = ctx.end();

    if (it != end) {
      if (*it == '}') presentation = 's';

      if ((*it == 'f') || (*it == 's')) presentation = *it++;
    }

    if (!presentation && (it != end) && (*it != '}')) throw format_error("invalid format");

    return it;
  }

  // parse is inherited from formatter<string>.
  template <typename FormatContext>
  auto format(const pierre::UUID &uuid, FormatContext &ctx) const -> decltype(ctx.out()) {
    const auto &s = uuid.storage;

    if (presentation == 'f') return fmt::format_to(ctx.out(), "{}", s);

    const std::string_view portion(s.begin() + s.find_last_of('-') + 1, std::prev(s.end()));
    return fmt::format_to(ctx.out(), "{}", portion);
  }
};
