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

#pragma once

#include <string>

namespace pierre {

struct ArgsMap {
  using string = std::string;

  bool parse_ok = false;
  bool daemon = false;
  string cfg_file{};
  string dmx_host{};
  string pid_file{};
  bool colorbars = false;

  // ArgsMap() = default;
  // ArgsMap(ArgsMap &&args_map) = default;
  // ArgsMap &operator=(ArgsMap &&args_mao) = default;
};

class Args {
public:
  using string = std::string;

public:
  typedef const char *cchar;

public:
  Args() = default;

  ArgsMap parse(int argc, char *argv[]);

private:
  cchar help_description = "Pierre is your light guy for any dance party.\n\n"
                           "Options";

  cchar daemon_arg = "daemon,b";
  cchar daemon_help = "daemon mode\nrun in background";
  cchar colorbars = "colorbars";
  cchar colorbars_help = "perform pinspot colorbar test at startup";
  cchar cfg_file_arg = "config,C";
  cchar cfg_file_default = "pierre.conf";
  cchar cfg_file_help = "config file";
  cchar dmx_host_arg = "dmx-host";
  cchar dmx_host_help = "stream dmx frames to host";
  cchar help = "help";
  cchar help_help = "help";
  cchar pid_file = "pid-file";
  cchar pid_file_help = R"(path
full path to write pid file when running as daemon
)";
};
} // namespace pierre