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
#include "base/typical.hpp"
#include "config/config.hpp"
#include "core/args.hpp"
#include "core/host.hpp"
#include "core/service.hpp"
#include "desk/desk.hpp"
#include "frame/frame.hpp"
#include "mdns/mdns.hpp"

namespace pierre {

Pierre::Pierre(const Inject &di)
    : di(di),                      //
      guard(io_ctx.get_executor()) //
{}

// create and run all threads
void Pierre::run() {
  auto host = Host::init();

  auto cfg = Config::init(                   //
      {.app_name = di.app_name,              //
       .cli_cfg_file = di.args_map.cfg_file, //
       .hostname = host->hostname()}         //
  );

  __LOG0(LCOL01 " {} {}\n", module_id, csv("RUN"), cfg->receiverName(), cfg->firmwareVersion());

  Service::init();
  mDNS::init()->start();
  Frame::init();
  Desk::init();

  // create and start Airplay
  Airplay::init();

  io_ctx.run();
}
} // namespace pierre
