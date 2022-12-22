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

#include "base/io.hpp"
#include "base/types.hpp"

#include <memory>

namespace pierre {

class App {
public:
  App() noexcept : guard(asio::make_work_guard(io_ctx)) {}

  int main(int argc, char *argv[]) noexcept;

private:
  // order dependent
  io_context io_ctx;
  work_guard guard;

  // order independent
  bool args_ok{false};

public:
  static constexpr csv module_id{"APP"};
};

} // namespace pierre
