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

#include "base/types.hpp"

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/thread_pool.hpp>
#include <memory>
#include <optional>

namespace pierre {

namespace asio = boost::asio;
using work_guard_tp = asio::executor_work_guard<asio::thread_pool::executor_type>;

class App {
public:
  App() = default;

  int main(int argc, char *argv[]);

private:
  void signals_ignore() noexcept;
  void signals_shutdown() noexcept;

private:
  // order dependent
  std::optional<asio::signal_set> signal_set_ignore;
  std::optional<asio::signal_set> signal_set_shutdown;

  // order independent
  std::optional<work_guard_tp> work_guard;
  bool args_ok{false};

public:
  static constexpr csv module_id{"app"};
};

} // namespace pierre
