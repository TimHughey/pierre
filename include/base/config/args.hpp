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

#include "base/config/toml.hpp"
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
  CliArgs(int argc, char **argv) noexcept : argv_0(argv[0]) {
    // get some base info and place into toml table
    cli_table.emplace("exec_path", fs::path(argv_0).remove_filename().string());
    cli_table.emplace("app_name", fs::path(argv_0).filename().string());
    cli_table.emplace("parent_path", fs::path(argv_0).parent_path().string());

    try {
      // Declare the supported options
      po::options_description desc("pierre");

      auto cfg_file_val =
          po::value<string>()
              ->notifier([this](const string file) { cli_table.emplace("cfg-file"sv, file); })
              ->default_value("live.toml");

      auto daemon_val =
          po::bool_switch()
              ->notifier([this](bool enable) { cli_table.emplace("daemon"sv, enable); })
              ->default_value(false);

      auto dmx_host_val =
          po::value<string>()
              ->notifier([this](const string host) { cli_table.emplace("dmx_host"sv, host); })
              ->default_value("dmx");

      auto force_restart_val =
          po::bool_switch()
              ->notifier([this](bool enable) { cli_table.emplace("force_restart"sv, enable); })
              ->default_value(false);

      auto pid_file_val =
          po::value<string>()
              ->notifier([this](const string file) { cli_table.emplace("pid_file", file); })
              ->default_value("/run/pierre/pierre.pid");

      auto log_file_val =
          po::value<string>()
              ->notifier([this](const string file) { cli_table.emplace("log_file", file); })
              ->default_value("/var/log/pierre/pierre.log");

      desc.add_options()                                                           //
          ("cfg-file,C", cfg_file_val, "config file name")                         //
          ("daemon,b", daemon_val, "run in background")                            //
          ("force-restart", force_restart_val, "force restart if already running") //
          ("dmx-host,D", dmx_host_val, "host to stream dmx frames")                //
          ("pid-file,P", pid_file_val, "full path to pid file")                    //
          ("log-file,L", log_file_val, "full path to log file")                    //
          ("help,h", "command line options overview");                             //

      // this will throw if parsing fails
      auto parsed_opts = po::parse_command_line(argc, argv, desc);

      // good, we parsed command line args, store them
      po::store(parsed_opts, args);

      // if help requested, print and exit cleanly
      if (args.count("help")) {
        std::cout << std::endl << desc << std::endl;

        exit(0);
      }

      // notify all args (populate toml table)
      po::notify(args);

    } catch (const po::error &ex) {
      fmt::print(std::clog, "command line args error: {}\n", ex.what());
      std::clog.flush();
      exit(1);
    }
  }

  bool daemon() const noexcept { return cli_table["daemon"sv].value_or(false); }
  bool force_restart() const noexcept { return cli_table["force_restart"sv].value_or(false); }
  std::any get_table() noexcept { return std::make_any<toml::table>(cli_table); }
  const string pid_file() const noexcept { return cli_table["pid_file"].ref<string>(); }

public:
  // order dependent
  toml::table cli_table;
  fs::path argv_0;

private:
  po::variables_map args;
};

} // namespace conf
} // namespace pierre