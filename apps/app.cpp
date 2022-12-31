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
#include "cals/config.hpp"
#include "cals/logger.hpp"
#include "cals/stats.hpp"
#include "desk/desk.hpp"
#include "mdns/mdns.hpp"
#include "rtsp/rtsp.hpp"

#include <signal.h>

int main(int argc, char *argv[]) {

  auto app = pierre::App();
  auto rc = app.main(argc, argv);

  exit(rc);
}

namespace pierre {

App::App() noexcept                        //
    : signal_set(io_ctx, SIGHUP, SIGINT),  //
      guard(asio::make_work_guard(io_ctx)) //
{}

void App::async_signal() noexcept {

  signal_set.async_wait([this](const error_code ec, int signal) {
    if (!ec) {
      INFO(module_id, "handle_signal", "caught signal={}\n", signal);

      if (signal == SIGABRT) {
        signal_set.clear();
        abort();
      } else if (signal == SIGINT) {

        Logger::teardown();

        Rtsp::shutdown();
        INFO(module_id, "async_signal", "rtsp shutdown complete\n");

        guard.reset();
        io_ctx.stop();
      } else {
        async_signal();
      }
    }
  });
}

int App::main(int argc, char *argv[]) {
  crypto::init(); // initialize sodium and gcrypt

  async_signal();

  // we'll start the entire app via the io_context
  asio::post(io_ctx, [&, this]() {
    Logger::init(io_ctx); // start logging, run on main process io_ctx

    auto cfg = Config::init(io_ctx, argc, argv);

    if (cfg->should_start()) {
      args_ok = true;
      INFO(module_id, "INIT", "{} {} {}\n", cfg->receiver(), cfg->build_vsn(), cfg->build_time());

      if (debug_init()) INFO(Config::module_id, "INIT", "{}\n", Config::ptr()->init_msg);

      Stats::init(io_ctx);
      mDNS::init();
      Desk::init(io_ctx);

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

  INFO(module_id, "init", "io_ctx has returned\n");

  return 0;
}

} // namespace pierre
