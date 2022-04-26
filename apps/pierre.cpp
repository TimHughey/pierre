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

#include <exception>
#include <fmt/format.h>
#include <iostream>
#include <list>
#include <thread>

#include "audio/dsp.hpp"
#include "audio/pcm.hpp"
#include "core/args.hpp"
#include "core/config.hpp"
#include "core/host.hpp"
#include "dmx/render.hpp"

#include "core/args.hpp"
#include "lightdesk/lightdesk.hpp"
#include "pierre.hpp"
#include "rtsp/rtsp.hpp"

namespace pierre {
using namespace std;

Pierre::Pierre(const string &_app_name, const ArgsMap &args_map)
    : cfg(_app_name, args_map.cfg_file) {
  // maybe more later?
}

Pierre::~Pierre() {}

// create and run all threads
void Pierre::run() {
  if (!cfg.findFile() || !cfg.load()) {
    throw(runtime_error("bad configuration file"));
  }

  // create shared ptrs to top level config providers and worker threads
  auto host = Host::create(cfg);
  auto rtsp = Rtsp::create(host);

  // start broadcast of AirPlay2 and handle audio streams
  rtsp->start();

  // wait for Rtsp to complete
  rtsp->join();

  fmt::print("Pierre: joined rtsp={}\n", fmt::ptr(rtsp.get()));

  // std::list<shared_ptr<thread>> threads;

  //   auto dsp = make_shared<audio::Dsp>();
  //   auto dmx = make_shared<dmx::Render>();
  //   auto lightdesk = make_shared<lightdesk::LightDesk>(dsp);

  //   threads.emplace_front(dsp->run());
  //   threads.emplace_front(dmx->run());

  //   lightdesk->saveInstance(lightdesk);
  //   threads.emplace_front(lightdesk->run());

  //   dmx->addProducer(lightdesk);

  //   sleep(10);

  //   if (State::leaving()) {
  //     lightdesk->leave();
  //   }

  //   State::shutdown();

  //   for (auto t : threads) {
  //     t->join();
  //   }

  //   cout << endl;
  // }
}
} // namespace pierre
