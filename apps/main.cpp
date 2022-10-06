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

#include "fmt/core.h"
#include <fmt/format.h>
#include <iostream>
#include <signal.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

#include "core/args.hpp"
#include "pierre.hpp"

using namespace pierre;

pierre_t pierre_instance;

void exitting() {
  using namespace std;

  pierre_instance.reset();

  cerr << endl << "farewell" << endl;
}

// for clean exits
void handleSignal(int signal) {
  using namespace std;

  switch (signal) {
  case SIGINT:
    cerr << endl << "caught SIGINT" << endl;
    exit(EXIT_SUCCESS);
    break;

  case SIGTERM:
    cerr << endl << "caught SIGINT" << endl;
    exit(EXIT_SUCCESS);
    break;

  default:
    cerr << endl << "caught signal: " << signal << " (ignored" << endl;
  }
}

int main(int argc, char *argv[]) {
  using namespace std;
  using namespace pierre;

  // we setup signal handling

  // control-c (SIGINT)
  struct sigaction act = {};
  act.sa_handler = handleSignal;
  sigaction(SIGINT, &act, NULL);

  // terminate (SIGTERM)
  struct sigaction act2 = {};
  act2.sa_handler = handleSignal;
  sigaction(SIGTERM, &act2, NULL);

  atexit(exitting);

  // what's our name?
  const auto app_name = string(basename(argv[0]));

  // parse args
  Args args;
  const auto args_map = args.parse(argc, argv);

  //
  if (args_map.ok() == false) {
    // either the parsing of args failed OR --help
    exit(1);
  }

  if (args_map.daemon) {
    fmt::print("main(): daemon requested\n");
  }

  pierre_instance = Pierre::create({.app_name = app_name, .args_map = args_map});
  pierre_instance->run();

  exit(0);
}

//   if (args.count(colorbars)) {
//     auto desk = State::config("lightdesk"sv);

//     desk->insert_or_assign("colorbars"sv, true);
//   }

//   if (args.count(dmx_host)) {
//     toml::table *dmx = State::config("dmx"sv);

//     dmx->insert_or_assign("host"sv, args[dmx_host].as<std::string>());
//   }

//   // auto pid = getpid();
//   // setpriority(PRIO_PROCESS, pid, -10);

//   auto pierre = make_shared<Pierre>();

//   pierre->run();

//   return 0;
// }

// int setupWorkingDirectory() {
//   auto base = State::config()->table()["pierre"sv];
//   fs::path dir;

//   if (base["use_home"sv].value_or(false)) {
//     dir.append(getenv("HOME"));
//   }

//   dir.append(base["working_dir"sv].value_or("/tmp"));

//   error_code ec;
//   fs::current_path(dir, ec);

//   if (ec) {
//     cerr << "unable to set working directory: " << ec.message() << endl;
//     return -1021;
//   }

//   return 0;
// }
