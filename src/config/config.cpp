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

#define TOML_IMPLEMENTATION // this translation unit compiles actual library
#include "config/config.hpp"
#include "base/elapsed.hpp"
#include "base/host.hpp"
#include "base/io.hpp"
#include "base/logger.hpp"
#include "base/pet.hpp"
#include "base/types.hpp"
#include "config/args.hpp"
#include "version.hpp"

#include <cstdlib>
#include <filesystem>
#include <fmt/chrono.h>
#include <iostream>
#include <latch>
#include <memory>

namespace pierre {

namespace {
namespace fs = std::filesystem;
}

// ArgsMap is truly static as it is set once at startup
// declare it here to avoid pulling in args_map.hpp into class header
static ArgsMap args_map;

// static class data
std::shared_ptr<Config> Config::self;

// Config API
Config::Config(io_context &io_ctx, int argc, char **argv) noexcept
    : io_ctx(io_ctx),               //
      file_timer(io_ctx),           //
      initialized(false),           //
      will_start(false),            //
      home_dir(std::getenv("HOME")) //
{
  Elapsed elapsed;

  args_map = Args().parse(argc, argv);

  if (args_map.ok() && !args_map.help) {
    full_path.append(home_dir).append(".pierre");
    // full_path /= ".pierre";
    full_path /= args_map.cfg_file;

    if (parse()) {
      std::error_code ec;
      last_write = fs::last_write_time(full_path, ec);

      initialized = true;
      will_start = true;

      if (table()["debug.init"sv])
        INFO(module_id, "INIT", "sizeof={} table size={} elapsed={}\n", //
             sizeof(Config),                                            //
             tables.front().size(),                                     //
             elapsed.humanize());
    }
  }
}

bool Config::has_changed(cfg_future &fut) noexcept {
  auto rc = false;

  if (fut.has_value() && fut->valid()) {
    if (auto status = fut->wait_for(0ms); status == std::future_status::ready) {
      rc = fut->get();
      fut.reset();
    }
  }

  return rc;
}

void Config::monitor_file() noexcept { // static

  file_timer.expires_after(1s);
  file_timer.async_wait([_s = ptr()](const error_code ec) {
    if (ec) return; // error or shutdown

    auto s = _s.get(); // minimize shared_ptr dereferences

    auto now_last_write = fs::last_write_time(s->full_path);

    if (auto diff = now_last_write - s->last_write; diff != Nanos::zero()) {
      INFOX(module_id, "MONITOR_FILE",
            "CHANGED file={0} "                          //
            "was={1:%F}T{1:%R%z} now={2:%F}T{2:%R%z}\n", //
            full_path.c_str(),                           //
            std::chrono::file_clock::to_sys(last_write),
            std::chrono::file_clock::to_sys(now_last_write));

      s->last_write = now_last_write;

      s->parse();

      if (s->change_proms) {
        s->change_proms->set_value(true);
        s->change_proms.reset();
        s->change_fut.reset();
      }
    }

    s->monitor_file();
  });
}

bool Config::parse() noexcept { // static
  auto rc = false;

  std::unique_lock lck(mtx, std::defer_lock);
  lck.lock();

  try {
    auto &table = tables.emplace_back(toml::parse_file(full_path.c_str()));

    // populate static build info
    auto base = table[BASE].as_table();
    base->emplace("build_vsn"sv, build::vsn);
    base->emplace("build_time"sv, build::timestamp);

    auto cli = table[CLI].as_table();
    cli->emplace("app_name"sv, args_map.app_name);
    cli->emplace("cfg_file"sv, args_map.cfg_file);
    cli->emplace("exec_path"sv, args_map.exec_path.c_str());
    cli->emplace("parent_path"sv, args_map.parent_path.c_str());

    rc = true;

    // is this a reload of the config?
    if (tables.size() == 2) {
      tables.pop_front(); // discard the old table
    }

  } catch (const toml::parse_error &err) {
    INFO(module_id, "ERROR", "parse failed file={} reason={}\n", full_path.c_str(),
         err.description());
  }

  return rc;
}

const string Config::receiver() noexcept {
  static constexpr csv fallback{"%h"};
  auto val = at("mdns.receiver"sv).value_or(fallback);

  return (val == fallback ? string(Host().hostname()) : string(val));
}

void Config::want_changes(cfg_future &chg_fut) noexcept {
  auto s = ptr();
  auto &proms = s->change_proms;
  auto &fut = s->change_fut;

  if (!proms.has_value() && !fut.has_value()) {
    proms.emplace();
    fut.emplace(proms->get_future());
  }

  chg_fut.emplace(fut.value());
}

} // namespace pierre