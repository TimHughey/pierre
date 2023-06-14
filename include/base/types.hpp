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

#include <concepts>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace pierre {

using namespace std::literals;

// string, string_view and const char *
using string = std::string;
using string_view = std::string_view;

typedef const std::string_view csv;

// Vector of Floats
using reals_t = std::vector<double>;

using seq_num_t = uint32_t; // frame sequence num
using ftime_t = uint32_t;   // frame timestamp

template <typename T, typename... U>
concept IsAnyOf = (std::same_as<T, U> || ...);

static constexpr uint16_t ANY_PORT{0};
using Port = uint16_t;

template <typename T>
concept IsFrame = requires(T &f) {
  { f.sn() } noexcept;
  { f.ts() } noexcept;
  { f.flush() } noexcept;
};

template <typename T>
concept AlwaysFalse = false;

using Peers = std::vector<string>;

template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};

#define MOD_ID(mid)                                                                                \
  static constexpr std::string_view module_id { mid }

} // namespace pierre
