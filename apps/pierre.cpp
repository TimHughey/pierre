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
#include "core/args.hpp"
#include "core/config.hpp"
#include "core/host.hpp"
#include "core/service.hpp"
#include "mdns/mdns.hpp"

#include <exception>
#include <fmt/format.h>
#include <list>
#include <thread>

namespace pierre {

// create and run all threads
void Pierre::run() {
  if (!cfg.findFile() || !cfg.load()) {
    throw(std::runtime_error("bad configuration file"));
  }

  // create core dependencies for injection
  auto host = Host::init({.cfg = cfg});
  __LOG0("{:<18} {} {}\n", moduleId, host->serviceName(), host->firmwareVerson());

  Service::init();

  mDNS::init()->start();

  // create and start Airplay
  auto &airplay_thread = Airplay::init()->run();

  airplay_thread.join();
}
} // namespace pierre
