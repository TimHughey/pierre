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
#include "version.h"

#include <filesystem>
#include <fmt/chrono.h>
#include <iostream>
#include <latch>
#include <memory>

namespace pierre {

namespace {
namespace fs = std::filesystem;
}

// static data outside of class
static io_context io_ctx;
static std::shared_ptr<work_guard> guard;
static steady_timer file_timer(io_ctx);
static ArgsMap args_map;

// class static member data
fs::path Config::full_path{"/home/thughey/.pierre"};
std::shared_mutex Config::mtx;
std::list<toml::table> Config::tables;
bool Config::initialized{false};
Threads Config::threads;
stop_tokens Config::tokens;
fs::file_time_type Config::last_write;
std::optional<std::promise<bool>> Config::change_proms;
std::optional<std::shared_future<bool>> Config::change_fut;

// Config API

// initialization
void Config::init_self(int argc, char **argv) noexcept {
  args_map = Args().parse(argc, argv);

  if (args_map.ok()) {
    full_path /= args_map.cfg_file;

    if (parse() && threads.empty()) {
      guard = std::make_shared<work_guard>(io_ctx.get_executor());

      std::latch latch(CONFIG_THREADS);

      // note: work guard created by constructor p
      for (auto n = 0; n < CONFIG_THREADS; n++) { // main thread is 0s
        threads.emplace_back([=, &latch](std::stop_token token) mutable {
          tokens.add(std::move(token));
          name_thread(TASK_NAME, n);
          latch.count_down();
          io_ctx.run();
        });
      }

      // caller thread waits until all tasks are started
      latch.wait();

      std::error_code ec;
      last_write = fs::last_write_time(full_path, ec);

      monitor_file();
      initialized = true;
    }

    INFO(module_id, "INIT", "sizeof={} threads={}/{}\n", //
         sizeof(Config), threads.size(), CONFIG_THREADS);
  }
}

bool Config::has_changed(std::shared_future<bool> &fut) noexcept {
  auto rc = false;

  if (fut.valid()) {
    if (auto status = fut.wait_for(0ms); status == std::future_status::ready) {
      rc = fut.get();
    }
  }

  return rc;
}

void Config::monitor_file() noexcept { // static

  file_timer.expires_after(1s);
  file_timer.async_wait([](const error_code ec) {
    if (!ec) {

      auto now_last_write = fs::last_write_time(full_path);

      if (auto diff = now_last_write - last_write; diff != Nanos::zero()) {
        INFO(module_id, "MONITOR_FILE",
             "CHANGED file={0} "                          //
             "was={1:%F}T{1:%R%z} now={2:%F}T{2:%R%z}\n", //
             full_path.c_str(),                           //
             std::chrono::file_clock::to_sys(last_write),
             std::chrono::file_clock::to_sys(now_last_write));

        last_write = now_last_write;

        parse();

        if (change_proms) {
          change_proms->set_value(true);
          change_proms.reset();
          change_fut.reset();
        }
      }
      monitor_file();
    }
  });
}

bool Config::parse() noexcept { // static
  auto rc = false;
  Elapsed elapsed;

  std::unique_lock lck(mtx, std::defer_lock);
  lck.lock();

  try {
    auto &table = tables.emplace_back(toml::parse_file(full_path.c_str()));

    // populate static build info
    auto base = table[BASE].as_table();
    base->emplace("build_vsn"sv, string(GIT_REVISION));
    base->emplace("build_time"sv, string(BUILD_TIMESTAMP));

    auto cli = table[CLI].as_table();
    cli->emplace("app_name"sv, args_map.app_name);
    cli->emplace("cfg_file"sv, args_map.cfg_file);

    rc = true;

    // is this a reload of the config?
    if (tables.size() == 2) {
      tables.pop_front(); // discard the old table
    }

  } catch (const toml::parse_error &err) {
    INFO(module_id, "ERROR", "parse failed file={} reason={}\n", full_path.c_str(),
         err.description());
  }

  INFO(module_id, "PARSE", "COMPLETE table size={} elapsed={}\n", tables.front().size(),
       elapsed.humanize());
  return rc;
}

const string Config::receiver() const noexcept {
  static constexpr csv fallback{"%h"};
  auto val = at("mdns.receiver"sv).value_or(fallback);

  return (val == fallback ? string(Host().hostname()) : string(val));
}

std::shared_future<bool> Config::want_changes() noexcept {

  if (!change_proms.has_value() && !change_fut.has_value()) {
    change_proms.emplace();
    change_fut.emplace(change_proms->get_future());
  }

  return *change_fut;
}

} // namespace pierre