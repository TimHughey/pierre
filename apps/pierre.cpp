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

Pierre::Pierre(const Inject &di)
    : cfg({.app_name = di.app_name, .cli_cfg_file = di.args_map.cfg_file}) {
  // maybe more later?
}

Pierre::~Pierre() {}

// create and run all threads
void Pierre::run() {
  if (!cfg.findFile() || !cfg.load()) {
    throw(std::runtime_error("bad configuration file"));
  }

  // create core dependencies for injection
  auto host = Host::init({.cfg = cfg});

  constexpr auto f = FMT_STRING("{} {} {} {}\n");
  fmt::print(f, runTicks(), fnName(), host->serviceName(), host->firmwareVerson());

  Service::init();

  mDNS::init()->start();

  // create and start Airplay
  auto &airplay_thread = Airplay::init()->run();

  airplay_thread.join();

  /* legacy
std::list<shared_ptr<thread>> threads;

  auto dsp = make_shared<audio::Dsp>();
  auto dmx = make_shared<dmx::Render>();
  auto lightdesk = make_shared<lightdesk::LightDesk>(dsp);

  threads.emplace_front(dsp->run());
  threads.emplace_front(dmx->run());

  lightdesk->saveInstance(lightdesk);
  threads.emplace_front(lightdesk->run());

  dmx->addProducer(lightdesk);

  sleep(10);

  if (State::leaving()) {
    lightdesk->leave();
  }

  State::shutdown();
  cout << endl;
  */
}
} // namespace pierre
