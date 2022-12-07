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

#include "args.hpp"
#include "base/logger.hpp"

#include <boost/program_options.hpp>
#include <filesystem>
#include <fmt/format.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
#include <vector>

namespace po = boost::program_options;
namespace fs = std::filesystem;

namespace pierre {

static constexpr ccs help_description{"Pierre"};

static constexpr ccs cfg_file_arg{"cfg-file"};
static constexpr ccs cfg_file_default{"live.toml"};
static constexpr ccs cfg_file_help{"config file"};
static constexpr ccs daemon_arg{"daemon"};
static constexpr ccs daemon_help{"run as daemon (background)"};
static constexpr ccs dmx_host_arg{"dmx-host"};
static constexpr ccs dmx_host_help{"where to stream dmx frames"};
static constexpr ccs help_help{"display this help message"};
static constexpr ccs help{"help"};
static constexpr ccs pid_file_help{"pid file path (daemon mode only)"};
static constexpr ccs pid_file_default("/run/pierre");
static constexpr ccs pid_file{"pid-file"};

ArgsMap Args::parse(int argc, char *argv[]) {
  po::variables_map args;
  ArgsMap am;

  try {
    // Declare the supported options
    po::options_description desc(help_description);

    auto daemon_val = po::bool_switch(&am.daemon)->default_value(false);
    auto cfg_file_val = po::value(&am.cfg_file)->default_value(cfg_file_default);
    auto dmx_host_val = po::value(&am.dmx_host);
    auto pid_file_val = po::value(&am.pid_file)->default_value(pid_file_default);

    desc.add_options()                              // add allowed cmd line opts
        (cfg_file_arg, cfg_file_val, cfg_file_help) // cfg file
        (dmx_host_arg, dmx_host_val, dmx_host_help) // dmx host
        (daemon_arg, daemon_val, daemon_help)       // run as daemon?
        (pid_file, pid_file_val, pid_file_help)     // pid file
        (help, help_help);                          // help

    po::store(parse_command_line(argc, argv, desc), args);
    po::notify(args);

    if (args.count(help) == 0) {
      am.parse_ok = true;
    } else {
      am.help = true;

      std::cout << std::endl;
      std::cout << desc;
      std::cout << std::endl;
    }

  } catch (const po::error &ex) {
    fmt::print("command line args error: {}\n", ex.what());
  }

  am.exec_path = string(argv[0]);
  am.parent_path = am.exec_path.parent_path();
  am.app_name.assign(basename(argv[0]));

  return am;
}

} // namespace pierre
