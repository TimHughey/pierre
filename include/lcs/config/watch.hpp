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
#include "lcs/config/token.hpp"
#include "lcs/config/toml.hpp"

#include <any>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system/error_code.hpp>
#include <concepts>
#include <filesystem>
#include <fmt/format.h>
#include <iterator>
#include <list>
#include <mutex>
#include <type_traits>

namespace pierre {

class Config;

namespace asio = boost::asio;
using error_code = boost::system::error_code;
using steady_timer = asio::steady_timer;
using strand_ioc = asio::strand<asio::io_context::executor_type>;

namespace config2 {
namespace {
namespace fs = std::filesystem;
}

using file_time_t = fs::file_time_type;

class watch {
public:
  watch(asio::io_context &io_ctx, Config *cfg_ptr) noexcept; // must be .cpp (incomplete types)
  ~watch() noexcept {
    [[maybe_unused]] error_code ec;
    file_timer.cancel(ec);
  }

  watch(watch &&) = default;
  watch &operator=(watch &&) = default;

  bool register_token(token &tok, token::lambda &&handler) noexcept;
  void unregister_token(token &tok) noexcept;

private:
  void check_file() noexcept;
  void monitor_file() noexcept;

private:
  // order dependent
  strand_ioc local_strand;
  steady_timer file_timer;
  Config *cfg_ptr;
  file_time_t file_last_time;

  // order independent
  std::mutex mtx;
  std::list<std::reference_wrapper<token>> tokens;

public:
  static constexpr csv module_id{"config.watch"};
};

} // namespace config2

} // namespace pierre