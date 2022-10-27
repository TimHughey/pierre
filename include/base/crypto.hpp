// Pierre - Custom audio processing for light shows at Wiss Landing
// Copyright (C) 2022  Tim Hughey
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
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "base/types.hpp"

#include <exception>
#include <fmt/format.h>
#include <gcrypt.h>
#include <sodium.h>

namespace pierre {

struct crypto {
  static constexpr auto vsn{"1.5.4"};

  static void init() {

    // initialize crypo libs
    if (sodium_init() < 0) {
      throw(std::runtime_error{"sodium_init() failed"});
    }

    if (gcry_check_version(vsn) == nullptr) {

      const string msg = fmt::format("outdated libcrypt, need {}\n", vsn);
      throw(std::runtime_error(msg));
    }

    gcry_control(GCRYCTL_DISABLE_SECMEM, 0);
    gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
  }
};

} // namespace pierre
