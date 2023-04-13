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

#include "base/types.hpp"

#include <any>
#include <chrono>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>

namespace pierre {
namespace config2 {

class watch;

namespace {
namespace fs = std::filesystem;
} // namespace

using file_time_t = fs::file_time_type;

struct token {

  using lambda = std::function<void(std::any &&next_table)>;

  // order dependent
  string mod_id;
  lambda handler = [](std::any &&) {};
  std::any table;
  watch *watch_raw{nullptr};

  token(const auto mod_id) noexcept : mod_id(mod_id.data()) {}
  ~token() noexcept;

  token(token &&) = default;
  token &operator=(token &&) = default;

  template <typename T> const T get() const noexcept { return std::any_cast<T>(table); }

  void notify_of_change(std::any &&next_table) noexcept;

  void registered(lambda &&ch, watch *watch_ptr) noexcept {
    handler = std::move(ch);
    watch_raw = watch_ptr;
  }

public:
  static constexpr csv module_id{"config.token"};
};

} // namespace config2
} // namespace pierre