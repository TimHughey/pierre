/*
Pierre - Custom audio processing for light shows at Wiss Landing
Copyright (C) 2021  Tim Hughey

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

#include <boost/program_options.hpp>
#include <iostream>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

#include "pierre.hpp"

using namespace std;
using namespace pierre;

bool parseArgs(int ac, char *av[], Pierre::Config &cfg);

int main(int argc, char *argv[]) {
  Pierre::Config cfg;

  if ((argc > 1) && (parseArgs(argc, argv, cfg) == false)) {
    exit(1);
  }

  // auto pid = getpid();
  // setpriority(PRIO_PROCESS, pid, -10);

  auto pierre = make_shared<Pierre>(cfg);

  pierre->run();

  return 255;
}

bool parseArgs(int ac, char *av[], Pierre::Config &cfg) {
  using namespace boost::program_options;
  namespace po = boost::program_options;

  const char *dmx = "dmx";
  const char *colorbars = "color-bars";

  try {
    // Declare the supported options.
    po::options_description desc("pierre options:");
    desc.add_options()("help", "display this help text")(
        dmx, po::value<string_t>(), "stream dmx frames to host")(
        colorbars, "pinspot color bar test at startup");

    po::variables_map vm;
    po::store(po::parse_command_line(ac, av, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
      cout << desc << "\n";
      return false;
    }

    if (vm.count(dmx)) {
      cfg.dmx.host = vm[dmx].as<std::string>();
    }

    if (vm.count(colorbars)) {
      cfg.lightdesk.colorbars.enable = true;
    }
  } catch (const error &ex) {
    cerr << ex.what() << endl;
  }

  return true;
}
