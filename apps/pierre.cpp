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

#include "pierre.hpp"
#include "airplay/airplay.hpp"
#include "base/crypto.hpp"
#include "base/host.hpp"
#include "base/logger.hpp"
#include "config/config.hpp"
#include "desk/desk.hpp"
#include "frame/frame.hpp"
#include "mdns/mdns.hpp"

namespace pierre {

Pierre &Pierre::init(int argc, char *argv[]) {

  crypto::init(); // initialize sodium and gcrypt
  Logger::init(); // start logging

  auto cfg = Config::init(argc, argv);

  if (cfg.ready()) {
    INFO(module_id, "CONSTRUCT", "{} {} {}\n", cfg.receiver(), cfg.build_vsn(), cfg.build_time());

    args_ok = true;
  }

  return *this;
}

// create and run all threads
bool Pierre::run() {

  if (args_ok) {
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
