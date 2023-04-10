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
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <memory>
#include <optional>

namespace pierre {

namespace asio = boost::asio;
using work_guard_ioc = asio::executor_work_guard<asio::io_context::executor_type>;

class App {
public:
  App() noexcept : work_guard(asio::make_work_guard(io_ctx)) {}

  int main(int argc, char *argv[]);

private:
  void signals_ignore() noexcept;
  void signals_shutdown() noexcept;

private:
  // order dependent
  std::optional<asio::signal_set> signal_set_ignore;
  std::optional<asio::signal_set> signal_set_shutdown;

  // order independent
  bool args_ok{false};
  asio::io_context io_ctx;
  work_guard_ioc work_guard;

public:
  static constexpr csv module_id{"app"};
};

} // namespace pierre
