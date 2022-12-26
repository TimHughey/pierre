
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

#include <array>
#include <fmt/format.h>
#include <pthread.h>
#include <thread>

namespace pierre {

using Threads = std::vector<Thread>;

inline void name_thread(csv name) noexcept {
  const auto tid = pthread_self();

  // name the avahi thread (if needed)
  std::array<char, 64> buff{};

  pthread_getname_np(tid, buff.data(), buff.size());

  if (csv(buff.data()) != name) {
    pthread_setname_np(tid, name.data());
  }
}

inline void name_thread(csv name, int num) noexcept {
  name_thread(fmt::format("{}_{}", name, num));
}

} // namespace pierre
