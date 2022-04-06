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

#include <iostream>
#include <signal.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

#include "core/args.hpp"
#include "pierre.hpp"

static pierre::Pierre *pierre_ptr = nullptr;

void exitting() {
  using namespace std;

  if (pierre_ptr)
    delete pierre_ptr;

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

  // we setup interactions with the OS then
  // immediately pass control to Pierre

  // control-c (SIGINT) cleanly
  struct sigaction act = {};
  act.sa_handler = handleSignal;
  sigaction(SIGINT, &act, NULL);

  // terminate (SIGTERM)
  struct sigaction act2 = {};
  act2.sa_handler = handleSignal;
  sigaction(SIGTERM, &act2, NULL);

  atexit(exitting);

  pierre_ptr = new pierre::Pierre();

  bool ok;
  pierre::ArgsMap args_map;

  tie(ok, args_map) = pierre_ptr->prepareToRun(argc, argv);

  if (args_map.daemon) {
    cerr << "want daemon" << endl;
  }

  if (ok) {
    pierre_ptr->run();
  } else {
    exit(1);
  }

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
