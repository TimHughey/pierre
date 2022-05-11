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

#include "typedefs.hpp"

namespace pierre {

struct ArgsMap {
  bool parse_ok = false;
  bool daemon = false;
  string cfg_file{};
  string dmx_host{};
  string pid_file{};
  bool colorbars = false;

  // public api
  bool ok() const { return parse_ok; }
};

class Args {
public:
  Args() = default;

  ArgsMap parse(int argc, char *argv[]);

private:
  ccs help_description = "Pierre is your light guy for any dance party.\n\n"
                         "Options";

  ccs daemon_arg = "daemon,b";
  ccs daemon_help = "daemon mode\nrun in background";
  ccs colorbars = "colorbars";
  ccs colorbars_help = "perform pinspot colorbar test at startup";
  ccs cfg_file_arg = "config,C";
  ccs cfg_file_default = "pierre.conf";
  ccs cfg_file_help = "config file";
  ccs dmx_host_arg = "dmx-host";
  ccs dmx_host_help = "stream dmx frames to host";
  ccs help = "help";
  ccs help_help = "help";
  ccs pid_file = "pid-file";
  ccs pid_file_help = R"(path
full path to write pid file when running as daemon
)";
};
} // namespace pierre