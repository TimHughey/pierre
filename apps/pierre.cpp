/*
    Pierre - Custom Light Show via DMX for Wiss Landing
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
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#include "pierre.hpp"
#include "airplay/airplay.hpp"
#include "base/crypto.hpp"
#include "base/host.hpp"
#include "base/logger.hpp"
#include "config/config.hpp"
#include "config2/config2.hpp"
#include "core/args.hpp"
#include "core/service.hpp"
#include "desk/desk.hpp"
#include "frame/frame.hpp"
#include "mdns/mdns.hpp"

namespace pierre {

Pierre &Pierre::init(int argc, char *argv[]) {

  // parse args
  Args args;
  const auto args_map = args.parse(argc, argv);

  if (args_map.ok()) {

    if (args_map.daemon) {
      fmt::print("main(): daemon requested\n");
    }

    crypto::init(); // initialize sodium and gcrypt
    Logger::init(); // start logging

    C2onfig::init();

    auto cfg = Config::init(                    //
        {.app_name = string(basename(argv[0])), //
         .cli_cfg_file = args_map.cfg_file,     //
         .hostname = Host().hostname()}         //
    );

    INFO(module_id, "CONSTRUCT", "{} {}\n", cfg->receiverName(), cfg->firmwareVersion());

    args_ok = true;
  }

  return *this;
}

// create and run all threads
bool Pierre::run() {

  if (args_ok) {
    Service::init();
    mDNS::init(io_ctx);
    Frame::init();
    Desk::init();

    // create and start Airplay
    Airplay::init();

    io_ctx.run();
  }

  return true;
}
} // namespace pierre
