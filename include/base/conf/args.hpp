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

#include "base/conf/keys.hpp"
#include "base/conf/toml.hpp"
#include "base/types.hpp"

#include <any>
#include <boost/program_options.hpp>
#include <cstdlib>
#include <filesystem>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <iostream>
#include <libgen.h>
#include <tuple>

namespace pierre {
namespace conf {
namespace {
namespace po = boost::program_options;
namespace fs = std::filesystem;
} // namespace

class CliArgs {

public:
  CliArgs(int argc, char **argv) noexcept : opts_desc("pierre") {
    // get some base info and place into toml table
    fs::path fs_arg0{argv[0]};

    // note: order dependent -- to get exec_path we must modify fs_arg0
    table.emplace(key::app_name, fs_arg0.filename().string());
    table.emplace(key::parent_dir, fs_arg0.parent_path().string());
    table.emplace(key::exec_dir, fs_arg0.remove_filename().string());

    try {
      auto cfg_file_val =
          po::value<string>()
              ->notifier([this](const string file) { table.emplace(key::cfg_file, file); })
              ->default_value("live.toml");

      auto daemon_val = po::bool_switch()
                            ->notifier([this](bool enable) { table.emplace(key::daemon, enable); })
                            ->default_value(false);

      auto dmx_host_val =
          po::value<string>()
              ->notifier([this](const string host) { table.emplace(key::dmx_host, host); })
              ->default_value("dmx");

      auto force_restart_val =
          po::bool_switch()
              ->notifier([this](bool enable) { table.emplace(key::force_restart, enable); })
              ->default_value(false);

      auto pid_file_val =
          po::value<string>()
              ->notifier([this](const string file) { table.emplace(key::pid_file_path, file); })
              ->default_value("/run/pierre/pierre.pid");

      auto log_file_val =
          po::value<string>()
              ->notifier([this](const string file) { table.emplace(key::log_file, file); })
              ->default_value("/var/log/pierre/pierre.log");

      auto help_val = po::value<bool>()
                          ->notifier([this](bool h) { table.emplace(key::help, h); })
                          ->default_value(false);

      opts_desc.add_options()                                                      //
          ("cfg-file", cfg_file_val, "config file name")                           //
          ("daemon", daemon_val, "run in background")                              //
          ("force-restart", force_restart_val, "force restart if already running") //
          ("dmx-host", dmx_host_val, "host to stream dmx frames")                  //
          ("pid-file", pid_file_val, "full path to pid file")                      //
          ("log-file", log_file_val, "full path to log file")                      //
          ("help,h", help_val, "command line options overview");                   //

      // this will throw if parsing fails
      auto parsed_opts = po::parse_command_line(argc, argv, opts_desc);

      // good, we parsed command line args, store them
      po::store(parsed_opts, args);

      // notify all args (populate toml table)
      po::notify(args);

    } catch (const po::error &ex) {
      error_str = fmt::format("command line args error: {}", ex.what());
    }
  }

  const string error_msg() const noexcept { return error_str; }

  const string help_msg() const noexcept {
    string msg;

    if (table[toml::path(key::help)].value_or(false)) {
      std::stringstream ss(msg);
      opts_desc.print(ss, 120);
    }

    return msg;
  }

  const auto &ttable() noexcept { return table; }

private:
  // order dependent
  po::options_description opts_desc;

  // order independent
  toml::table table;
  string error_str;

  po::variables_map args;
};

} // namespace conf
} // namespace pierre