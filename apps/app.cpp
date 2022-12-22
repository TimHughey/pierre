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

#include "app.hpp"
#include "base/crypto.hpp"
#include "base/host.hpp"
#include "base/logger.hpp"
#include "config/config.hpp"
#include "desk/desk.hpp"
#include "mdns/mdns.hpp"
#include "rtsp/rtsp.hpp"
#include "stats/stats.hpp"

int main(int argc, char *argv[]) {

  auto app = pierre::App();
  auto rc = app.main(argc, argv);

  exit(rc);
}

namespace pierre {

int App::main(int argc, char *argv[]) noexcept {
  crypto::init(); // initialize sodium and gcrypt

  // we'll start the entire app via the io_context
  asio::post(io_ctx, [&, this]() {
    Logger::init(io_ctx); // start logging, run on main process io_ctx

    auto cfg = Config::init(io_ctx, argc, argv);

    if (cfg->should_start()) {
      args_ok = true;
      INFO(module_id, "INIT", "{} {} {}\n", cfg->receiver(), cfg->build_vsn(), cfg->build_time());

      Stats::init(io_ctx);
      mDNS::init();
      Desk::init();

      // create and start RTSP
      Rtsp::init();

      cfg->monitor_file();
    } else {
      // app is not starting, teardown Logger and remove the work guard
      Logger::teardown();
      guard.reset();
    }
  });

  io_ctx.run(); // start the app
  Rtsp::shutdown();

  return 0;
}

} // namespace pierre
