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

#include "base/conf/watch.hpp"
#include "base/conf/fixed.hpp"
#include "base/logger.hpp"

#include <error.h>
#include <fmt/std.h>
#include <sys/inotify.h>

namespace pierre {
namespace conf {

namespace fs = std::filesystem;

watch::~watch() noexcept {

  inotify_rm_watch(w_fd, in_fd);
  close(in_fd);
}

bool watch::changed() noexcept {
  LOG_CAT_AUTO("changed");

  bool rc{false};

  if (in_fd && w_fd) {
    std::array<char, sizeof(inotify_event)> buff;

    // read from inotify fd (this will not block)
    // watch fd is included in inotify_event
    auto bytes = read(in_fd, buff.data(), buff.size());

    if (bytes == buff.size()) {

      auto *in_evt = reinterpret_cast<inotify_event *>(buff.data());

      if (in_evt->wd == w_fd) {

        last_write_at = fs::last_write_time(file_path);
        rc = true;
        INFO_AUTO("{} {}", file_path);

        parse();
      }
    }
  }

  return rc;
}

bool watch::initiate() noexcept {
  constexpr auto fn_id{"initiate"sv};

  file_path = make_path();

  if (in_fd > 0) {
    INFO_AUTO("already initiated, in_fd={}", in_fd);
    return false;
  }

  if (in_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC); in_fd < 0) {
    INFO_AUTO("inotify_init1() failed, reason={}", std::strerror(errno));
    return false;
  }

  const auto flags{IN_CLOSE_WRITE | IN_MOVE_SELF};
  if (w_fd = inotify_add_watch(in_fd, file_path.c_str(), flags); w_fd < 0) {
    INFO_AUTO("inotify_add_watch() failed, reason={}", std::strerror(errno));
    return false;
  }

  INFO_AUTO("in_fd={} w_fd={} {}", in_fd, w_fd, file_path);

  return true;
}

} // namespace conf
} // namespace pierre