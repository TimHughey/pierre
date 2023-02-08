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
#include "io/io.hpp"
#include "lcs/types.hpp"

#include <filesystem>
#include <future>
#include <memory>
#include <mutex>

namespace pierre {

class ConfigWatch;

namespace shared {
extern std::unique_ptr<ConfigWatch> config_watch;
}

class ConfigWatch {
public:
  ConfigWatch(io_context &io_ctx) noexcept;
  ~ConfigWatch() noexcept;

  bool has_changed(cfg_future &want_fut) noexcept;

  void want_changes(cfg_future &want_fut) noexcept;
  const auto watch_msg() const noexcept { return _watch_msg; }

private:
  void file_watch() noexcept;

private:
  // order dependent
  io_context &io_ctx;
  steady_timer file_timer;
  const string file_path;

  // order independent
  cfg_future watch_fut;
  std::filesystem::file_time_type last_changed;
  std::optional<std::promise<bool>> prom;
  std::mutex mtx;
  string _watch_msg;

public:
  static constexpr csv module_id{"config_watch"};
};

////
//// free functions
////

inline auto cfg_watch_has_changed(cfg_future &want_fut) noexcept {
  return shared::config_watch->has_changed(want_fut);
}

inline void cfg_watch_want_changes(cfg_future &want_fut) noexcept {
  shared::config_watch->want_changes(want_fut);
}

} // namespace pierre