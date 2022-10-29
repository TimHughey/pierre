// Pierre
// Copyright (C) 2021  Tim Hughey
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
// along with this program.  If not, see <https://www.gnu.org/licenses/>

#include "pierre.hpp"

int main(int argc, char *argv[]) {

  pierre::Pierre().init(argc, argv).run();

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
