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

#include "conf/watch.hpp"
#include "base/logger.hpp"
#include "conf/fixed.hpp"
#include "conf/parser.hpp"
#include "conf/toml.hpp"

#include <algorithm>
#include <boost/asio/append.hpp>
#include <error.h>
#include <fmt/chrono.h>
#include <fmt/std.h>
#include <sys/inotify.h>

namespace pierre {
namespace conf {

namespace fs = std::filesystem;

conf::watch *watch::self{nullptr};

watch::watch(asio::io_context &app_io_ctx) noexcept
    : io_ctx(app_io_ctx), poll_timer(io_ctx, 5s), last_write_at(0s),
      cfg_file(fixed::cfg_file().c_str()) //
{
  // save ourself for easy singleton access
  self = this;

  msgs[Init] = fmt::format("sizeof={:5}", sizeof(watch));

  asio::post(io_ctx, [this]() {
    const auto init_flags{IN_NONBLOCK | IN_CLOEXEC};
    if (fd_in = inotify_init1(init_flags); fd_in >= 0) {

      const auto add_flags{IN_CLOSE_WRITE | IN_MOVE_SELF};
      if (fd_w = inotify_add_watch(fd_in, cfg_file.c_str(), add_flags); fd_w < 0) {

        msgs[Watch] = fmt::format("inotify_add_watch() failed, reason={}", std::strerror(errno));
      }

    } else {
      msgs[Watch] = fmt::format("inotify_init1() failed, reason={}", std::strerror(errno));
    }
  });
}

watch::~watch() noexcept {
  inotify_rm_watch(fd_w, fd_in);
  close(fd_in);
}

void watch::check() noexcept {
  INFO_AUTO_CAT("check");

  msgs[Watch].clear();

  if (fd_in && fd_w) {
    std::array<char, sizeof(inotify_event)> buff;

    // read from inotify fd (this will not block)
    // watch fd is included in inotify_event
    auto bytes = read(fd_in, buff.data(), buff.size());

    if (bytes == buff.size()) {
      auto *in_evt = reinterpret_cast<inotify_event *>(buff.data());

      if (in_evt->wd == fd_w) {
        last_write_at = fs::last_write_time(cfg_file);

        toml::table tt_next;

        if (parse(tt_next, msgs) && (tt_next != ttable)) {
          ttable = std::move(tt_next);

          msgs[Info] = fmt::format("{} table_size={}", fixed::cfg_file(), ttable.size());

          INFO_AUTO("{}", msgs[Info]);

          std::unique_lock lck(tokens_mtx);

          for (auto &tokc : tokens) {
            if (watches.contains(tokc.uuid)) {
              tokc.changed(true);

              INFO_AUTO("notified {}", tokc.uuid);
            }
          }
        }
      }
    }

    const auto next_at = schedule();
    INFO_AUTO("next at {}", next_at);
  }
}

void watch::initiate_watch(const token &tokc) noexcept {
  INFO_AUTO_CAT("initiate_watch");

  std::unique_lock lck(tokens_mtx);
  auto [it, inserted] = watches.emplace(tokc.uuid);

  INFO_AUTO("registered={} {}", inserted, tokc);
}

token &watch::make_token(csv mid) noexcept {
  std::unique_lock lck(tokens_mtx);

  return tokens.emplace_back(mid, self);
}

void watch::release_token(token &tokc) noexcept {
  std::unique_lock lck(tokens_mtx);

  std::erase_if(tokens, [this, &tokc](auto &t) { return tokc.uuid == t.uuid; });
}

} // namespace conf
} // namespace pierre