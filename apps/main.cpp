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
namespace po = boost::program_options;

bool parseArgs(int ac, char *av[], Pierre::Config &cfg);

int main(int argc, char *argv[]) {
  Pierre::Config cfg;

  cout << "\033[2J";
  cout << "Hello, this is Pierre." << endl;

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

  // Declare the supported options.
  po::options_description desc("pierre options:");
  desc.add_options()("help", "display this help text")(
      "dmx", po::value<string_t>(), "stream dmx frames")(
      "dB-floor-peak", po::value<float>(), "register peaks > dB")(
      "dB-floor-bass", po::value<float>(), "register bass > dB")(
      "color-scale-min", po::value<float>(), "color scaling range minimum")(
      "color-scale-max", po::value<float>(), "color scaling range maximum")(
      "desk-dB-floor", po::value<float>(), "lightdesk major peak dB floor");

  po::variables_map vm;
  po::store(po::parse_command_line(ac, av, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    cout << desc << "\n";
    return false;
  }

  if (vm.count("dmx")) {
    cfg.dmx.host = vm["dmx"].as<std::string>();
  }

  if (vm.count("peak-dB-floor")) {
    cfg.dsp.floor.peak_dB = vm["dB-floor-peak"].as<float>();
  }

  if (vm.count("bass-dB-floor")) {
    cfg.dsp.floor.bass_dB = vm["dB-floor-bass"].as<float>();
  }

  if (vm.count("color-scale-min")) {
    cfg.lightdesk.color.scale.min = vm["color-scale-min"].as<float>();
  }

  if (vm.count("color-scale-max")) {
    cfg.lightdesk.color.scale.max = vm["color-scale-max"].as<float>();
  }

  if (vm.count("desk-dB-floor")) {
    cfg.lightdesk.majorpeak.dB_floor = vm["desk-dB-floor"].as<float>();
  }

  return true;
}
