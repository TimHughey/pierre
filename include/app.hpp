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
#include "base/conf/args.hpp"
#include "base/conf/token.hpp"
#include "base/types.hpp"

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/signal_set.hpp>
#include <memory>
#include <optional>

namespace pierre {

class App {
public:
  App(int argc, char *argv[]) noexcept;

  int main();

private:
  void check_pid_file() noexcept;
  void signals_ignore() noexcept;
  void signals_shutdown() noexcept;
  void write_pid_file() noexcept;

private:
  // order dependent
  std::optional<asio::signal_set> signal_set_ignore;
  std::optional<asio::signal_set> signal_set_shutdown;
  conf::CliArgs cli_args;
  asio::io_context io_ctx;
  conf::token ctoken;

  // order independent
  bool args_ok{false};

public:
  static constexpr csv module_id{"app"};
};

} // namespace pierre
