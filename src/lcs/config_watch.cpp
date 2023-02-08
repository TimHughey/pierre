// Pierre
// Copyright (C) 2023 Tim Hughey
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

#include "lcs/config_watch.hpp"
#include "base/pet.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"

#include <fmt/std.h>

namespace pierre {
namespace shared {
std::unique_ptr<ConfigWatch> config_watch;
}

ConfigWatch::ConfigWatch(io_context &io_ctx) noexcept
    : io_ctx(io_ctx),                        //
      file_timer(io_ctx),                    //
      file_path(shared::config->file_path()) //
{
  std::error_code ec;
  last_changed = fs::last_write_time(file_path, ec);

  if (!ec) file_watch();

  INFO_INIT("sizeof={:>5} file_watch={}\n", sizeof(ConfigWatch), !ec);
}

ConfigWatch::~ConfigWatch() noexcept {
  try {
    file_timer.cancel();
  } catch (...) {
  }
}

void ConfigWatch::file_watch() noexcept {
  static constexpr csv fn_id{"file_watch"};

  file_timer.expires_after(1s);
  file_timer.async_wait([this](const error_code ec) {
    if (ec) return; // error or shutdown, bail now!

    const auto file_path = shared::config->file_path();
    auto now_last_changed = fs::last_write_time(file_path);

    if (auto diff = now_last_changed - last_changed; diff != Nanos::zero()) {
      _watch_msg = fmt::format(                          //
          "{0} {1:%F}T{1:%R} {2}",                       //
          file_path,                                     //
          std::chrono::file_clock::to_sys(last_changed), //
          pet::humanize(diff));

      INFO_AUTO("{}\n", _watch_msg);

      last_changed = now_last_changed;

      shared::config->parse();

      std::unique_lock lck(mtx, std::defer_lock);
      lck.lock();

      if (prom) { // notify watchers
        prom->set_value(true);
        prom.reset();
        watch_fut.reset();
      }

      lck.unlock();
    }

    file_watch();
  });
}

bool ConfigWatch::has_changed(cfg_future &fut) noexcept {
  auto rc = false;

  if (fut.has_value() && fut->valid()) {
    if (auto status = fut->wait_for(0ms); status == std::future_status::ready) {
      rc = fut->get();
      fut.reset();
    }
  }

  return rc;
}

void ConfigWatch::want_changes(cfg_future &want_fut) noexcept {
  std::unique_lock lck(mtx, std::defer_lock);
  lck.lock();

  // create our watch prom and future, if needed
  if (!prom.has_value() && !watch_fut.has_value()) {
    prom.emplace();
    watch_fut.emplace(prom->get_future());
  }

  // set the caller's future to our shared watch future
  want_fut.emplace(watch_fut.value());
}

} // namespace pierre