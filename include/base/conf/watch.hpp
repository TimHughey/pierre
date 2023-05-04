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

#include "base/asio.hpp"
#include "base/conf/token.hpp"
#include "base/conf/toml.hpp"
#include "base/pet_types.hpp"
#include "base/types.hpp"

#include <array>
#include <boost/asio/async_result.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/post.hpp>
#include <concepts>
#include <filesystem>
#include <fmt/format.h>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

namespace pierre {
namespace conf {

class watch {
  friend struct fmt::formatter<watch>;
  friend class token;

public:
  using f_time = std::filesystem::file_time_type;
  using fs_path = std::filesystem::path;
  using stc_tp = std::chrono::steady_clock::time_point;
  using work_guard_ioc = asio::executor_work_guard<asio::io_context::executor_type>;

public:
  watch() noexcept;
  ~watch() noexcept;

  void initiate_watch(const token &tokc) noexcept;

  std::future<toml::table> latest() noexcept {
    auto prom = std::promise<toml::table>();
    auto fut = prom.get_future();

    asio::post(io_ctx, asio::append([this](std::promise<toml::table> p) { p.set_value(ttable); },
                                    std::move(prom)));

    return fut;
  }

  const string &msg(ParseMsg id) const noexcept { return msgs[id]; }

  auto schedule(Millis freq_ms = 5000ms) noexcept {
    const auto next_at = system_clock::now() + freq_ms;

    poll_timer.expires_at(next_at);
    poll_timer.async_wait([this](const error_code &ec) {
      if (ec) return;

      check();
    });

    return next_at;
  }

protected:
  token *make_token(csv mid) noexcept;
  void release_token(const string &uuid) noexcept;

private:
  void check() noexcept;

protected:
  // order dependent
  asio::io_context io_ctx;
  asio::system_timer poll_timer;
  f_time last_write_at;
  const string cfg_file;
  std::mutex tokens_mtx;
  // std::unique_lock<std::mutex> lck;

  // order independent
  std::jthread thread;
  toml::table ttable;
  int fd_in{-1}; // inotify init
  int fd_w{-1};  // inotify add

  parse_msgs_t msgs;

  std::vector<std::unique_ptr<token>> tokens;
  std::set<string> watches;

  static watch *self;

public:
  static constexpr auto module_id{"conf.watch"sv};
};

} // namespace conf
} // namespace pierre

template <> struct fmt::formatter<pierre::conf::watch> : formatter<std::string> {

  // parse is inherited from formatter<string>.
  template <typename FormatContext>
  auto format(const pierre::conf::watch &w, FormatContext &ctx) const {

    auto msg_count =
        std::count_if(w.msgs.begin(), w.msgs.end(), [](auto &msg) { return !msg.empty(); });

    const auto msg =
        fmt::format("{} fd_in={} fd_w={} msgs={}", w.cfg_file, w.fd_in, w.fd_w, msg_count);

    return formatter<std::string>::format(std::move(msg), ctx);
  }
};