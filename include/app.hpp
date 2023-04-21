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
#include <memory>
#include <thread>

namespace pierre {

class App : public std::enable_shared_from_this<App> {
public:
  App() noexcept;

  static auto create() { return std::make_shared<App>(); }

  void main();

private:
  // order dependent
  asio::io_context io_ctx;
  asio::signal_set ss_shutdown;

  /// order independent
  std::jthread thread;
  string err_msg;

public:
  static constexpr csv module_id{"app"};
};

} // namespace pierre
