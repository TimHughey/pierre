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
#include <thread>
#include <type_traits>
#include <vector>

namespace pierre {

using namespace std::literals;

// string, string_view and const char *
using string = std::string;
typedef const std::string &csr;

using string_view = std::string_view;
typedef const std::string_view csv;
typedef const char *ccs;

// threads
using Thread = std::jthread;

// Vector of Floats
using reals_t = std::vector<double>;

using reel_serial_num_t = uint64_t;
using seq_num_t = uint32_t;   // frame sequence num
using timestamp_t = uint32_t; // frame timestamp

template <typename T, typename... U>
concept IsAnyOf = (std::same_as<T, U> || ...);

template <class> inline constexpr bool always_false_v = false;

using Port = uint16_t;

} // namespace pierre
