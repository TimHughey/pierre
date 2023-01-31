// Pierre
// Copyright (C) 2022 Tim Hughey
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
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#pragma once

#include "base/threads.hpp"
#include "base/types.hpp"
#include "io/io.hpp"
#include "lcs/args.hpp"

namespace pierre {

class LoggerConfigStats {

public:
  LoggerConfigStats() = default;
  void init(io_context &io_ctx, CliArgs cli_args) noexcept;

  void shutdown() noexcept;

private:
  // order dependent
  // io_context &io_ctx;

  // io_context io_ctx;
  // work_guard guard;

  // order independent
  // Threads threads;

public:
  static constexpr csv module_id{"lcs"};
};

} // namespace pierre