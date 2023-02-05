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
#include "lcs/config.hpp"
#include "base/elapsed.hpp"
#include "base/host.hpp"
#include "base/pet.hpp"
#include "base/types.hpp"
#include "build_version.hpp"
#include "io/io.hpp"
#include "lcs/args.hpp"

#include <cstdlib>
#include <filesystem>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <iostream>
#include <latch>
#include <memory>

namespace pierre {

namespace {
namespace fs = std::filesystem;
}

namespace shared {
std::unique_ptr<Config> config{nullptr};
}

// Config API
Config::Config(io_context &io_ctx, const toml::table &cli_table) noexcept
    : io_ctx(io_ctx),                           //
      cli_table(cli_table), file_timer(io_ctx), //
      initialized(false)                        //
{}

Config::~Config() noexcept {

  try {
    file_timer.cancel();
  } catch (...) {
  }
}

const string Config::app_name() noexcept {
  return config()->cli_table["app_name"sv].value<string>().value();
}

const string Config::banner_msg() noexcept {
  return fmt::format("{} {} {}", app_name(), build_vsn(), build_time());
}

const string Config::build_time() noexcept {
  return config()->at(base_path("build_time")).value_or(UNSET);
}

const string Config::build_vsn() noexcept {
  return config()->at(base_path("build_vsn")).value_or(UNSET);
}

bool Config::daemon() noexcept { return config()->cli_table["daemon"sv].value_or(false); }

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

void Config::init() noexcept {
  // we jave good cli args, build path to config file
  full_path = fs::path(cli_table["home"sv].ref<string>())
                  .append(".pierre")
                  .append(cli_table["cfg-file"sv].ref<string>());

  if (parse(EXIT_ON_FAILURE) == true) {

    std::error_code ec;
    last_write = fs::last_write_time(full_path, ec);

    initialized = true;

    init_msg = fmt::format("sizeof={:>4} table size={}", //
                           sizeof(Config), tables.front().size());

    // if we made it here cli and config are parsed, toml tables are ready for use so
    // begin watching for changes
    monitor_file();
  }
}

bool Config::info_bool(csv mod, csv cat) noexcept {
  if (cat == csv{"info"}) return true;

  auto path = toml::path("info"sv);

  // order of precedence:
  //  1. info.<cat>       == boolean
  //  2. info.<mod>       == boolean
  //  3. info.<mod>.<cat> == boolean
  if (live()[toml::path(path).append(cat)].is_boolean()) {
    return live()[path.append(cat)].ref<bool>();
  } else if (live()[toml::path(path).append(mod)].is_boolean()) {
    return live()[path.append(mod)].ref<bool>();
  } else {
    return live()[path.append(mod).append(cat)].value_or(true);
  }
}

const toml::table &Config::live() noexcept {
  std::shared_lock slk(mtx, std::defer_lock);
  slk.lock();

  return tables.front();
}

void Config::monitor_file() noexcept {

  file_timer.expires_after(1s);
  file_timer.async_wait([this](const error_code ec) {
    if (ec) return; // error or shutdown, bail immediately

    auto now_last_write = fs::last_write_time(full_path);

    if (auto diff = now_last_write - last_write; diff != Nanos::zero()) {
      monitor_msg.clear();
      auto w = std::back_inserter(monitor_msg);
      fmt::format_to(w,
                     "CHANGED file={0} "                        //
                     "was={1:%F}T{1:%R%z} now={2:%F}T{2:%R%z}", //
                     full_path.c_str(),                         //
                     std::chrono::file_clock::to_sys(last_write),
                     std::chrono::file_clock::to_sys(now_last_write));

      last_write = now_last_write;

      parse();

      std::unique_lock lck(want_changes_mtx, std::defer_lock);
      lck.lock();

      if (change_proms) {
        change_proms->set_value(true);
        change_proms.reset();
        change_fut.reset();
      }

      lck.unlock();
    }

    monitor_file();
  });
}

bool Config::parse(bool exit_on_error) noexcept {

  std::unique_lock lck(mtx, std::defer_lock);
  if (!exit_on_error) lck.lock();

  try {
    auto &table = tables.emplace_back(toml::parse_file(full_path.c_str()));

    // populate static build info
    auto base = table["base"].as_table();
    base->emplace("build_vsn"sv, build::vsn);
    base->emplace("build_time"sv, build::timestamp);

    // is this a reload of the config?
    if (tables.size() == 2) {
      tables.pop_front(); // discard the old table
    }

  } catch (const toml::parse_error &err) {

    parse_msg = fmt::format("parse failed file={} reason={}", full_path.c_str(), err.description());

    if (exit_on_error) {
      fmt::print("\n{}\n", parse_msg);
      exit(2);
    }
  }

  return true;
}

bool Config::ready() noexcept { return config() && config()->initialized; }

const string Config::receiver() noexcept { // static
  static constexpr csv fallback{"%h"};
  auto val = config()->at("mdns.receiver"sv).value_or(fallback);

  return (val == fallback ? string(Host().hostname()) : string(val));
}

const string Config::vsn() noexcept {
  return config()->live()["base.config_vsn"sv].value_or(UNSET);
}

void Config::want_changes(cfg_future &chg_fut) noexcept {
  auto *s = config();

  std::unique_lock lck(s->want_changes_mtx, std::defer_lock);
  lck.lock();

  auto &proms = s->change_proms;
  auto &fut = s->change_fut;

  if (!proms.has_value() && !fut.has_value()) {
    proms.emplace();
    fut.emplace(proms->get_future());
  }

  chg_fut.emplace(fut.value());
}

} // namespace pierre