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

#include "airplay/common/ap_inject.hpp"
#include "core/typedefs.hpp"

#include <list>
#include <memory>
#include <optional>
#include <stop_token>
#include <thread>
#include <unordered_map>

namespace pierre {

class Airplay {
private:
  typedef std::list<Thread> Threads;
  static constexpr auto MAX_THREADS() { return 5; };

public:
  Airplay() = default;

  Thread &start(const airplay::Inject &di);
  void teardown(Thread &thread) { thread.request_stop(); }

private:
  void run();

private:
  std::optional<const airplay::Inject> di_storage;
  Threads threads;
  bool running = false;
};

} // namespace pierre
