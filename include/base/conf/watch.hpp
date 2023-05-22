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

/// @brief See conf::token for further details
///
///        conf::watch provides notification of changes to the
///        on-disk configuration file for conf::tokens that
///        opt-in.
///
///        conf::watch member functions are marked protected because
///        it should only be used embedded with a conf::token
///
///        The only exception is access to parser messages during
///        startup and before Logger is started.
///
///        conf::watch creates a separate thread to detect changes
///        to the on-disk configuration file periodically and maintains a list
///        of all conf::watch objects that are notified when a
///        change is detected.
///
///        It is essential that release() is called when the owner of the
///        conf::watch is destroyed to prevent exceptions while attempting
///        to notify.
///
///        The configuration file is watched by a single inotify therefore
///        any change to the file is notified.  How to handle notifies for
///        configurations not changed are left to the user as an
///        usage / implementation detail.
///
///        Change detection and notification are thread safe.

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

protected:
  /// @brief called once per owning object creation
  ///        see conf::token
  /// @param tokc conf::token to watch for configuration changes
  void initiate_watch(const token &tokc) noexcept;

  /// @brief Provide the latest configuration via a future
  ///        Unless watch is busy reading the configuration file
  ///        the promise is immediately set a value.
  /// @return Future consisting of the latest configuration
  std::future<toml::table> latest() noexcept {
    auto prom = std::promise<toml::table>();
    auto fut = prom.get_future();

    asio::post(io_ctx, asio::append([this](std::promise<toml::table> p) { p.set_value(ttable); },
                                    std::move(prom)));

    return fut;
  }

public:
  /// @brief Return the message string for the requested id
  /// @param id message id
  /// @return const string reference
  const string &msg(ParseMsg id) const noexcept { return msgs[id]; }

protected:
  /// @brief Schedule the next check of for configuration changes
  ///        with thread safely
  /// @param freq_ms duration until check is performed
  /// @return std::chrono::timepoint the next check will be performed
  auto schedule(Millis freq_ms = 1000ms) noexcept {
    const auto next_at = system_clock::now() + freq_ms;

    poll_timer.expires_at(next_at);
    poll_timer.async_wait([this](const error_code &ec) {
      if (ec) return;

      check();
    });

    return next_at;
  }

protected:
  /// @brief Create a conf::token with embedded watch ensuring thread safety.
  ///
  ///        Design note: the conf token is created, posted to the watch io_context
  ///        and returned to the caller.  As a consequence, a minor race condition is
  ///        possible between creation and notification for an in-progress change
  ///        detection.
  /// @param mid module_id (aka configuration root)
  /// @return pointer to the newly created conf::token
  token *make_token(csv mid) noexcept;

  /// @brief Sister method to release a token created by make_token
  ///        must be called by owner object at destruction
  ///
  ///        Releasing a token is thread safe.
  /// @param tokc
  void release_token(const token *tokc) noexcept;

private:
  /// @brief Internal member function to detect if a configuration change has occurred
  void check() noexcept;

protected:
  // order dependent
  asio::io_context io_ctx;
  asio::system_timer poll_timer;
  f_time last_write_at;
  const string cfg_file;
  std::mutex tokens_mtx;

  // order independent
  std::jthread thread;
  toml::table ttable;
  int fd_in{-1}; // inotify init
  int fd_w{-1};  // inotify add

  parse_msgs_t msgs;

  std::vector<std::unique_ptr<token>> tokens;
  std::set<UUID> watches;

  static watch *self;

public:
  MOD_ID("conf.watch");
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