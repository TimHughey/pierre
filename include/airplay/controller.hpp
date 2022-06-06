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

#include "airplay/common/typedefs.hpp"

#include <boost/asio.hpp>
#include <fmt/format.h>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <pthread.h>
#include <thread>

namespace pierre {
namespace airplay {

class Controller;
typedef std::shared_ptr<Controller> shController;

namespace shared {
std::optional<shController> &controller();
} // namespace shared

namespace { // anonymous constrains to this file
namespace asio = boost::asio;
}

class Controller : public std::enable_shared_from_this<Controller> {
private:
  typedef std::list<Thread> Threads;

public:
  // shared instance management
  static shController init() {
    return shared::controller().emplace(new Controller())->shared_from_this();
  }
  static shController ptr() { return shared::controller().value()->shared_from_this(); }
  static void reset() { shared::controller().reset(); }

  void join() { airplay_thread.join(); }
  void teardown() {}
  Thread &start();

private:
  Controller() : watchdog_timer(io_ctx) {}

  void kickstart();

  void nameThread(auto num) {
    const auto handle = pthread_self();
    const auto name = fmt::format("AirPlay {:<02}", num);

    pthread_setname_np(handle, name.c_str());
  }

  void run();
  void watchDog();

private:
  // order depdendent
  io_context io_ctx; // run by multiple threads
  asio::high_resolution_timer watchdog_timer;

  // std::once_flag _kickstart_;

  Thread airplay_thread;

  // order independent
  Threads threads;
  bool running = false;
};

} // namespace airplay
} // namespace pierre
