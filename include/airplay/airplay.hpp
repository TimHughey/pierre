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

#include "base/io.hpp"
#include "base/threads.hpp"
#include "base/types.hpp"

#include <memory>

namespace pierre {

class Airplay : public std::enable_shared_from_this<Airplay> {
private:
  static constexpr int AIRPLAY_THREADS{4};

private:
  Airplay() : watchdog_timer(io_ctx), guard(io::make_work_guard(io_ctx)) {}

public:
  // shared instance management
  static void init() noexcept;

  auto ptr() noexcept { return shared_from_this(); }
  static void reset() noexcept { self().reset(); }

  // void join() { thread_main.join(); }

  // void shutdown() {
  //   thread_main.request_stop();
  //   ranges::for_each(threads, [](Thread &t) { t.join(); });
  //   thread_main.join();
  // }

private:
  void init_self() noexcept;

  // DRAGONS BE HERE!!
  // returns reference to ACTUAL shared_ptr holding Airplay
  static std::shared_ptr<Airplay> &self() noexcept;

  void watch_dog() noexcept;

private:
  // order depdendent
  io_context io_ctx; // run by multiple threads
  steady_timer watchdog_timer;
  work_guard_t guard;

  // order independent
  Threads threads;
  stop_tokens tokens;

public:
  static constexpr csv module_id{"AIRPLAY"};
};

} // namespace pierre
