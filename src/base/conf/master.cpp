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

#include "base/conf/master.hpp"
#include "base/conf/cli_args.hpp"
#include "base/conf/toml.hpp"
#include "base/pet.hpp"
#include "build_inject.hpp"

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

std::shared_ptr<master> mptr;

namespace fs = std::filesystem;
using file_clock = std::chrono::file_clock;
using file_time = fs::file_time_type;
using fs_path = fs::path;

// config file path and last write
static fs_path cff_path;
static file_time cff_last_write;
toml::table master::ttable;

master::master(int argc, char *argv[]) noexcept {

  // first, parse the command line args
  CliArgs cli_args(argc, argv);

  msgs[HelpMsg] = cli_args.help_msg();
  msgs[ArgsErrMsg] = cli_args.error_msg();

  // when there aren't any error messages proceed with parsing
  if (std::all_of(msgs.begin(), msgs.end(), [](const auto &m) { return m.empty(); })) {

    // populate the cli args and build info before parsing
    ttable.emplace(root::cli, CliArgs::table());
    ttable.emplace(root::build, build_info::ttable());

    // populate the git info into the build table
    auto bt = ttable[root::build].as<toml::table>();
    bt->emplace(key::git_describe, build::git.describe);

    parse();

    msgs[InitMsg] = fmt::format("sizeof={:>5} table_size={}", sizeof(master), ttable.size());
  }
}

const string &master::get_first_msg() const noexcept {

  static const string empty{"no first message"};

  auto it = std::find_if(msgs.begin(), msgs.end(), [](auto &m) { return !m.empty(); });

  if (it < msgs.end()) return *it;

  return empty;
}

bool master::nominal_start() const noexcept {
  return std::any_of(msgs.begin(), msgs.end(), [](auto &m) { return !m.empty(); });
}

// void master::check_file() noexcept {
//   static constexpr csv fn_id{"check_file"};

//   auto now_last_changed = fs::last_write_time(cff_path);

//   if (auto diff = now_last_changed - cff_last_write; diff > Nanos::zero()) {

//     monitor_msg = fmt::format(              //
//         "{0} {1:%F}T{1:%R} {2}\n",          //
//         cff_path,                           //
//         file_clock::to_sys(cff_last_write), //
//         pet::humanize(diff));

//     cff_last_write = now_last_changed;

//     if (parse()) {
//       std::scoped_lock lck(tokens_mtx);

//       std::for_each(tokens.begin(), tokens.end(), [this](const auto &tok_ref) mutable {
//         auto &tok = tok_ref.get();

//         auto next_table = copy_from_master(mod_id);

//         tok.notify_of_change(std::move(next_table));
//       });
//     }
//   }

//   monitor_file();
// }

// void token::monitor_file() noexcept {

//   file_timer->expires_from_now(1s);
//   file_timer->async_wait([this](const error_code ec) {
//     if (ec) return; // error or shutdown, bail...

//     check_file();
//   });
// }

// void token::notify_of_change(std::any &&next_table) noexcept {
//   static constexpr csv fn_id{"notify"};

//   const auto &table0 = std::any_cast<toml::table>(table);
//   const auto &table1 = std::any_cast<toml::table>(next_table);

//   if (table0 != table1) {
//     const auto &h = std::any_cast<lambda>(handler);

//     h(std::forward<std::any>(next_table));
//   }
// }

bool master::parse() noexcept {

  msgs[ParseMsg].clear();

  // we only build the config file path once
  if (cff_path.empty()) {
    const auto bt = ttable[root::build];
    const auto ct = ttable[root::cli];

    cff_path = fs::path{bt[key::sysconf_dir].ref<string>()};
    cff_path /= bt[key::project].ref<string>();
    cff_path /= ct[key::cfg_path].ref<string>();
  }

  auto pt_result = toml::parse_file(cff_path.c_str());

  if (pt_result) {
    const auto &t = pt_result.table();

    // merge the parsed config to our local table
    t.for_each(
        [this](const toml::key &key, auto &&val) { ttable.insert_or_assign(key, std::move(val)); });

  } else {
    const auto &error = pt_result.error();
    msgs[ParseMsg] = fmt::format("{} parse failed: {}", cff_path, error.description());
  }

  return parse_ok();
}

} // namespace conf
} // namespace pierre
