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

int main(int argc, char *argv[]) {
  pierre::Pierre().init(argc, argv).run();

  exit(0);
}

namespace pierre {

Pierre &Pierre::init(int argc, char *argv[]) noexcept {

  crypto::init(); // initialize sodium and gcrypt
  Logger::init(); // start logging

  auto cfg = Config::init(argc, argv);

  if (cfg.should_start()) {
    args_ok = true;
    INFO(module_id, "INIT", "{} {} {}\n", cfg.receiver(), cfg.build_vsn(), cfg.build_time());
  } else {
    // app is not starting, teardown Logger and remove the work guard
    Logger::teardown();
    guard.reset();
  }

  return *this;
}

// create and run all threads
void Pierre::run() noexcept {

  if (args_ok && Config().ready()) {
    mDNS::init(io_ctx);
    Frame::init();
    Desk::init();

    // create and start Airplay
    Airplay::init();

    io_ctx.run();
  }
}
} // namespace pierre
