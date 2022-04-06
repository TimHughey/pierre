/*
Pierre - Custom audio processing for light shows at Wiss Landing
Copyright (C) 2022  Tim Hughey

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>. */

#include <boost/format.hpp>
#include <boost/program_options.hpp>

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#include "args.hpp"

using namespace std;
using namespace pierre;
using namespace boost::program_options;
namespace fs = std::filesystem;
using string = std::string;

namespace pierre {

ArgsMap Args::parse(int argc, char *argv[]) {
  using namespace boost::program_options;

  variables_map args;

  ArgsMap args_map;

  try {
    // Declare the supported options.
    options_description desc(help_description);

    auto daemon_val = bool_switch(&args_map.daemon)->multitoken()->default_value(false);
    auto cfg_file_val = value(&args_map.cfg_file)->multitoken()->default_value(cfg_file_default);
    auto dmx_host_val = value(&args_map.dmx_host)->multitoken();
    auto pid_file_val = value(&args_map.pid_file);
    auto colorbars_val = bool_switch(&args_map.colorbars)->default_value(false);

    desc.add_options()                              // add allowed cmd line opts
        (help, help_help)                           // help
        (daemon_arg, daemon_val, daemon_help)       // run as daemon?
        (cfg_file_arg, cfg_file_val, cfg_file_help) // cfg file
        (dmx_host_arg, dmx_host_val, dmx_host_help) // dmx host
        (pid_file, pid_file_val, pid_file_help)     // pid file
        (colorbars, colorbars_val, colorbars_help); // run colorbar test at startup

    store(parse_command_line(argc, argv, desc), args);
    notify(args);

    if (args.count(help) == 0) {
      args_map.parse_ok = true;
    } else {
      cout << desc << "\n";
    }

  } catch (const error &ex) {
    cerr << ex.what() << endl;
  }

  return forward<ArgsMap>(args_map);
}

} // namespace pierre
