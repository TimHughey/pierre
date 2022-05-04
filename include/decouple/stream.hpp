
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
//
//  This work based on and inspired by
//  https://github.com/mikebrady/nqptp Copyright (c) 2021--2022 Mike Brady.

#pragma once

#include <array>
#include <cstdint>

namespace pierre {

class Stream {
public:
  enum Type : uint8_t {
    Unknown = 0,
    Uncompressed, // L16/44100/2
    Lossless
  };

  typedef std::array<uint8_t, 16> Aes;
  typedef std::array<int32_t, 12> Fmtp;

private:
  bool encrypted = false;
  Aes iv{0};
  Aes key{0};
  Fmtp fmtp{0};
  Type type = Unknown;
};

} // namespace pierre