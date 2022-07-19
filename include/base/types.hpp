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

#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace pierre {

using namespace std::literals;

// string, string_view and const char *
using string = std::string;
typedef const string &csr;

using string_view = std::string_view;
typedef const string_view csv;
typedef const char *ccs;

// threads
typedef std::jthread Thread;

// Vector of Floats
typedef std::vector<float> Reals;

// airplay frame sequence num
typedef uint32_t SeqNum;

// Frequency, magnitude, scaled values,
typedef float Freq;
typedef float Mag;
typedef float MagScaled;
typedef float Scaled;
typedef float Unscaled;

// stream is playing or not playing
constexpr csv NOT_PLAYING{"NOT PLAYING"};
constexpr csv PLAYING{"PLAYING"};

} // namespace pierre
