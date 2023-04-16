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

#define TOML_IMPLEMENTATION

#include "base/config/token.hpp"
#include "base/config/toml.hpp"
#include "base/pet.hpp"
#include "build_inject.hpp"
#include "git_version.hpp"

#include <algorithm>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <filesystem>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/std.h>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace pierre {
namespace conf {

using steady_timer = asio::steady_timer;
using strand_ioc = asio::strand<asio::io_context::executor_type>;

// class static members
std::any token::cli_tbl;
std::any token::master_tbl;
token::tokens_t token::tokens; // the created tokens
std::mutex token::tokens_mtx;  // guard access to tokens

string token::init_msg;
string token::monitor_msg;
string token::parse_msg;

std::unique_ptr<strand_ioc> local_strand{nullptr};
std::unique_ptr<steady_timer> file_timer{nullptr};

namespace fs = std::filesystem;
using file_clock = std::chrono::file_clock;
using file_time = fs::file_time_type;
using fs_path = fs::path;

// config file path and last write
static fs_path cff_path;
static file_time cff_last_write;

token::token(csv mid) noexcept : mod_id(mid.data()) {

  table = std::move(copy_from_master(mod_id));
  emplace_token();
}

token::token(csv mid, asio::io_context &io_ctx, std::any &&cli_table) noexcept
    : mod_id{mid.data()} {

  cli_tbl = std::move(cli_table);

  if (parse()) {

    table = std::move(copy_from_master(mod_id));

    if (!local_strand || !file_timer) {
      cff_last_write = fs::last_write_time(cff_path);

      local_strand = std::make_unique<strand_ioc>(asio::make_strand(io_ctx));
      file_timer = std::make_unique<steady_timer>(*local_strand, steady_clock::now());

      monitor_file();
    }

    emplace_token();
  }
}

token::~token() noexcept {
  std::scoped_lock lck(tokens_mtx);

  auto found_it = std::find_if(tokens.begin(), tokens.end(),
                               [this](auto &t) { return t.get().mod_id == mod_id; });

  if (found_it != tokens.end()) tokens.erase(found_it);

  if (local_strand && tokens.empty()) {
    auto stopped = local_strand->get_inner_executor().context().stopped();

    if (stopped) {
      if (file_timer) file_timer.reset();
      if (local_strand) local_strand.reset();
    }
  }
}

const string token::build_vsn() noexcept {
  const auto &t = std::any_cast<toml::table>(master_tbl);

  return t["git_describe"_tpath].ref<string>();
}

void token::check_file() noexcept {
  static constexpr csv fn_id{"check_file"};

  auto now_last_changed = fs::last_write_time(cff_path);

  if (auto diff = now_last_changed - cff_last_write; diff > Nanos::zero()) {

    monitor_msg = fmt::format(              //
        "{0} {1:%F}T{1:%R} {2}\n",          //
        cff_path,                           //
        file_clock::to_sys(cff_last_write), //
        pet::humanize(diff));

    cff_last_write = now_last_changed;

    if (parse()) {
      std::scoped_lock lck(tokens_mtx);

      std::for_each(tokens.begin(), tokens.end(), [this](const auto &tok_ref) mutable {
        auto &tok = tok_ref.get();

        auto next_table = copy_from_master(mod_id);

        tok.notify_of_change(std::move(next_table));
      });
    }
  }

  monitor_file();
}

std::any token::copy_from_master(csv mid) noexcept { // static

  const toml::path sub_path{mid};
  const toml::table &t = std::any_cast<toml::table>(master_tbl);

  if (t[sub_path].is_table()) {
    // note:  not a reference, makes a copy
    const auto sub_table = t[sub_path].ref<toml::table>();

    return std::make_any<toml::table>(std::move(sub_table));
  }

  return std::make_any<toml::table>();
}

void token::emplace_token() noexcept {
  std::scoped_lock lck(tokens_mtx);

  tokens.emplace_back(std::ref(*this));
}

bool token::daemon() noexcept {
  const auto &t = std::any_cast<toml::table>(cli_tbl);

  return t["daemon"sv].value_or(false);
}

const string token::data_path() noexcept {
  const auto &t = std::any_cast<toml::table>(master_tbl);
  return string(t["data_dir"sv].ref<string>());
}

void token::monitor_file() noexcept {

  file_timer->expires_from_now(1s);
  file_timer->async_wait([this](const error_code ec) {
    if (ec) return; // error or shutdown, bail...

    check_file();
  });
}

void token::notify_of_change(std::any &&next_table) noexcept {
  static constexpr csv fn_id{"notify"};

  const auto &table0 = std::any_cast<toml::table>(table);
  const auto &table1 = std::any_cast<toml::table>(next_table);

  if (table0 != table1) {
    const auto &h = std::any_cast<lambda>(handler);

    h(std::forward<std::any>(next_table));
  }
}

bool token::parse() noexcept {

  parse_msg.clear();

  if (cff_path.empty()) {
    const auto &cli_table = std::any_cast<toml::table>(cli_tbl);

    cff_path = fs::path(build::info.sysconf_dir) //
                   .append(build::info.project)  //
                   .append(cli_table["cfg-file"_tpath].ref<string>());
  }

  auto pt_result = toml::parse_file(cff_path.c_str());

  if (pt_result) {
    // store the successful parsing into the std::any
    auto &t = table.emplace<toml::table>(pt_result.table());

    t.emplace("project", build::info.project);
    t.emplace("git_describe", build::git.describe);
    t.emplace("install_prefix", build::info.install_prefix);
    t.emplace("sysconf_dir"sv, build::info.sysconf_dir);
    t.emplace("data_dir", build::info.data_dir);

    master_tbl = std::make_any<toml::table>(t);

  } else {
    const auto &error = pt_result.error();
    parse_msg = fmt::format("{} parse failed: {}", cff_path, error.description());
  }

  return parse_msg.empty();
}

} // namespace conf
} // namespace pierre
