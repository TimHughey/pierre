// Pierre
// Copyright (C) 2021  Tim Hughey
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
// along with this program.  If not, see <https://www.gnu.org/licenses/>

#pragma once

#include "base/asio.hpp"
#include "base/types.hpp"

#include <boost/asio/signal_set.hpp>
#include <boost/asio/system_timer.hpp>
#include <memory>
#include <stop_token>
#include <thread>

namespace pierre {

/// @brief Pierre Application Object
class App {
public:
  /// @brief Construct the App object.
  ///        CLI arguments have already been handled and
  ///        the application is on the cusp of starting.
  App() noexcept;

  /// @brief Similar to 'C' main.  Performs all initialization,
  ///        starts subsystems and runs io_ctx.  Returns when
  ///        io work is exhausted (primarily by a shutdown request).
  void main();

private:
  /// @brief Starts a timer to periodically watch for a stop request
  /// @param stoken stop_token returned by thread that runs io_ctx
  void stop_request_watcher(std::stop_token stoken) noexcept;

private:
  // order dependent
  asio::io_context io_ctx;
  asio::signal_set ss_shutdown;

  /// order independent
  std::jthread thread;
  string err_msg;

public:
  MOD_ID("app");
};

} // namespace pierre
