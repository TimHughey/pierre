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

#include "conf/cli_args.hpp"
#include "base/types.hpp"
#include "build_inject.hpp"
#include "conf/fixed.hpp"
#include "conf/keys.hpp"
#include "conf/toml.hpp"

#include <boost/program_options.hpp>
#include <filesystem>
#include <fmt/format.h>
#include <fmt/std.h>
#include <iostream>
#include <sstream>

namespace pierre {
namespace conf {

namespace po = boost::program_options;
namespace fs = std::filesystem;

using fs_path = fs::path;

// there will only ever be a single collection of cli args per invocation
static po::options_description desc("pierre");
static po::variables_map args;

constexpr auto def_cfg_toml_file{"live.toml"};
constexpr auto def_chdir{"/"};
constexpr auto def_daemon{false};
constexpr auto def_dmx_host{"dmx"};
static const string def_log_file{"/var/log/pierre/pierre.log"};
constexpr auto def_pid_file{"/run/pierre/pierre.pid"};
constexpr auto def_restart{false};

constexpr auto desc_daemon{"detach and run in background"};
constexpr auto desc_dmx_host{"stream dmx frames to this host"};
constexpr auto desc_restart{"force restart (if running)"};
constexpr auto desc_cfg_file{"toml configuration file"};
constexpr auto desc_chdir{"cd to this directory at app start"};
constexpr auto desc_help{"command line help"};
constexpr auto desc_log_file{"full path to log file"};
constexpr auto desc_pid_file{"full path to pid file"};
constexpr auto opt_help{"help"};

// class static data
toml::table cli_args::ttable;
string cli_args::error_str;
bool cli_args::help_requested{false};
std::ostringstream cli_args::help_ss;

cli_args::cli_args(int argc, char **argv) noexcept {
  // get some base info and place into toml table
  fs_path fs_arg0{argv[0]};

  // note: order dependent -- to get exec_path we must modify fs_arg0
  ttable.emplace(key::app_name, fs_arg0.filename().string());
  ttable.emplace(key::parent_dir, fs_arg0.parent_path().string());
  ttable.emplace(key::exec_dir, fs_arg0.remove_filename().string());

  fs_path def_cfg_fs_file(build::info.sysconf_dir);
  def_cfg_fs_file /= def_cfg_toml_file;

  auto cfg_file_v = po::value<string>()
                        ->notifier([this](const string p) {
                          fs_path p_fs(p);

                          if (p_fs.is_absolute()) {
                            ttable.emplace<string>(key::cfg_file, p);
                          } else {
                            p_fs = fs_path(build::info.sysconf_dir).append(p);

                            ttable.emplace<string>(key::cfg_file, p_fs.string());
                          }
                        })
                        ->default_value(def_cfg_fs_file.string());

  auto daemon_v = po::bool_switch()
                      ->notifier([this](bool e) { ttable.emplace(key::daemon, e); })
                      ->default_value(def_daemon);

  auto dmx_host_v = po::value<string>()
                        ->notifier([this](const string h) { ttable.emplace(key::dmx_host, h); })
                        ->default_value(def_dmx_host);

  auto restart_v = po::bool_switch()
                       ->notifier([this](bool e) { ttable.emplace(key::force_restart, e); })
                       ->default_value(def_restart);

  auto pid_file_v = po::value<string>()
                        ->notifier([this](const string f) { ttable.emplace(key::pid_file, f); })
                        ->default_value(def_pid_file);

  auto log_file_v = po::value<string>()
                        ->notifier([this](const string f) { ttable.emplace(key::log_file, f); })
                        ->default_value(def_log_file);

  auto help_v = po::bool_switch()
                    ->notifier([this](bool e) { ttable.emplace(key::help, e); })
                    ->default_value(false);

  desc.add_options()                                //
      (key::cfg_file, cfg_file_v, desc_cfg_file)    //
      (key::daemon, daemon_v, desc_daemon)          //
      (key::force_restart, restart_v, desc_restart) //
      (key::dmx_host, dmx_host_v, desc_dmx_host)    //
      (key::pid_file, pid_file_v, desc_pid_file)    //
      (key::log_file, log_file_v, desc_log_file)    //
      (opt_help, help_v, desc_help);                //

  try {
    // this will throw if parsing fails
    auto parsed_opts = po::parse_command_line(argc, argv, desc);

    // good, we parsed command line args, store them
    po::store(parsed_opts, args);

    // notify all args (populate toml table)
    po::notify(args);

  } catch (const po::error &ex) {
    error_str = fmt::format("bad args: {}", ex.what());
  }

  if (ttable[key::help].value_or(false)) {
    help_requested = true;

    desc.print(std::cout);
  }

  if (!fs::exists(conf::fixed::cfg_file())) {
    error_str = fmt::format("{}: not found\n", conf::fixed::cfg_file());
  }
}

} // namespace conf
} // namespace pierre