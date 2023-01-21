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

#include "base/types.hpp"

#include <boost/program_options.hpp>
#include <cstdlib>
#include <filesystem>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <iostream>
#include <libgen.h>

#define TOML_ENABLE_FORMATTERS 0 // don't need formatters
#define TOML_HEADER_ONLY 0       // reduces compile times
#include <toml++/toml.h>

namespace pierre {
namespace {
namespace po = boost::program_options;
namespace fs = std::filesystem;
} // namespace

class CliArgs {
public:
  CliArgs(int argc, char **argv) noexcept : argv_0(argv[0]) {
    // get some base info and place into toml table
    cli_table.emplace("home"sv, string(std::getenv("HOME")));
    cli_table.emplace("pid"sv, getpid());
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
              ->notifier([this](const string host) { cli_table.emplace("dmx-host"sv, host); })
              ->default_value("dmx");

      auto pid_file_val =
          po::value<string>()
              ->notifier([this](const string file) { cli_table.emplace("pid-file", file); })
              ->default_value("/run/pierre/pierre.pid");

      desc.add_options()                                                    //
          ("cfg-file,C", cfg_file_val, "config file name")                  //
          ("daemon,b", daemon_val, "run in background")                     //
          ("dmx-host, D", dmx_host_val, "host to stream dmx frames")        //
          ("pid-file,P", pid_file_val, "full path where to write pid file") //
          ("help,h", "command line options overview");                      //

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

public:
  // order dependent
  toml::table cli_table;
  fs::path argv_0;

private:
  po::variables_map args;
};

} // namespace pierre