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

// static class data
std::shared_ptr<Config> Config::self;

// Config API
Config::Config(io_context &io_ctx, toml::table &cli_table) noexcept
    : io_ctx(io_ctx),      //
      file_timer(io_ctx),  //
      initialized(false),  //
      cli_table(cli_table) // make a copy of the cli_table
{
  // we jave good cli args, build path to config file

  full_path = fs::path(cli_table["home"sv].ref<string>())
                  .append(".pierre")
                  .append(cli_table["cfg-file"sv].ref<string>());

  if (parse(EXIT_ON_FAILURE) == true) {
    std::error_code ec;
    last_write = fs::last_write_time(full_path, ec);

    initialized.store(true);

    init_msg = fmt::format("sizeof={} table size={}", //
                           sizeof(Config), tables.front().size());
  }
}

void Config::init(io_context &io_ctx,
                  toml::table &cli_table) noexcept { // static
  if (self.use_count() < 1) {
    self = std::shared_ptr<Config>(new Config(io_ctx, cli_table));
    auto s = self.get();

    s->parse(EXIT_ON_FAILURE);

    // if we made it here cli and config are parsed, toml tables are ready for use so
    // begin watching for changes

    s->banner_msg = //
        fmt::format("{} {} {} pid={}", s->receiver(), s->build_vsn(), s->build_time(),
                    s->cli_table["pid"sv].ref<int64_t>());

    s->monitor_file();
  }
}

void Config::monitor_file() noexcept {

  file_timer.expires_after(1s);
  file_timer.async_wait([s = ptr()](const error_code ec) {
    if (ec) return; // error or shutdown, bail immediately

    auto now_last_write = fs::last_write_time(s->full_path);

    if (auto diff = now_last_write - s->last_write; diff != Nanos::zero()) {
      s->monitor_msg.clear();
      auto w = std::back_inserter(s->monitor_msg);
      fmt::format_to(w,
                     "CHANGED file={0} "                        //
                     "was={1:%F}T{1:%R%z} now={2:%F}T{2:%R%z}", //
                     s->full_path.c_str(),                      //
                     std::chrono::file_clock::to_sys(s->last_write),
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

bool Config::parse(bool exit_on_error) noexcept {

  std::unique_lock lck(mtx, std::defer_lock);
  if (!exit_on_error) lck.lock();

  try {
    auto &table = tables.emplace_back(toml::parse_file(full_path.c_str()));

    // populate static build info
    auto base = table[BASE].as_table();
    base->emplace("build_vsn"sv, build::vsn);
    base->emplace("build_time"sv, build::timestamp);

    // is this a reload of the config?
    if (tables.size() == 2) {
      tables.pop_front(); // discard the old table
    }

  } catch (const toml::parse_error &err) {

    parse_msg = fmt::format("parse failed file={} reason={}", full_path.c_str(), err.description());

    if (exit_on_error) {
      fmt::print(std::clog, "\n{}\n", parse_msg);
      exit(2);
    }
  }

  return true;
}

const string Config::receiver() noexcept { // static
  static constexpr csv fallback{"%h"};
  auto val = self->at("mdns.receiver"sv).value_or(fallback);

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