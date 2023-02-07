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

#include "base/thread_util.hpp"

#include <array>
#include <fmt/format.h>
#include <iterator>
#include <pthread.h>

namespace pierre {

const string thread_util::set_name(csv name, int num) noexcept {
  static constexpr csv prefix{"pie"};
  const auto tid = pthread_self();

  string thread_name;
  auto w = std::back_inserter(thread_name);

  fmt::format_to(w, "{}_{}", prefix, name);

  if (num >= 0) fmt::format_to(w, "{}", num);

  // name the avahi thread (if needed)
  std::array<char, 64> buff{0x00};

  pthread_getname_np(tid, buff.data(), buff.size());

  if (csv(buff.data()) != csv(thread_name)) {
    pthread_setname_np(tid, thread_name.c_str());
  }

  return thread_name;
}

} // namespace pierre