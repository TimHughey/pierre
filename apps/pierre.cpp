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

#include <iostream>
#include <thread>
#include <unordered_set>

#include "audio/dsp.hpp"
#include "audio/pcm.hpp"
#include "core/args.hpp"
#include "core/host.hpp"
#include "dmx/render.hpp"
#include "fmt/format.h"
#include "lightdesk/lightdesk.hpp"
#include "mdns/mdns.hpp"
#include "pierre.hpp"
#include "rtsp/rtsp.hpp"

namespace pierre {
using namespace std;

tuple<bool, ArgsMap> Pierre::prepareToRun(int argc, char *argv[]) {
  Args args;
  auto args_map = args.parse(argc, argv);

  auto ok = false;

  if (args_map.parse_ok) {
    _cfg = std::make_shared<Config>();

    auto config_ok = _cfg->findFile(args_map.cfg_file) && _cfg->load();

    if (config_ok) {
      // _cfg->test("pierre", "description");
      ok = true;
    }
  }

  return make_tuple(ok, args_map);
}

void Pierre::run() {
  unordered_set<shared_ptr<thread>> threads;

  auto host = Host::create(); // returns shared pointer

  const auto mdns_opts = mDNS::Opts{
      .host = host, .service_base = _cfg->service_name, .firmware_vsn = _cfg->firmware_version};

  auto mdns = mDNS::create(mdns_opts); // returns shared_ptr
  mdns->start();                       // mDNS uses Avahi created and managed thread

  auto rtsp = make_shared<Rtsp>(_cfg->port);
  threads.insert(make_shared<thread>([rtsp]() { rtsp->run(); }));

  auto dsp = make_shared<audio::Dsp>();
  auto dmx = make_shared<dmx::Render>();
  auto lightdesk = make_shared<lightdesk::LightDesk>(dsp);

  threads.insert(dsp->run());
  threads.insert(dmx->run());

  lightdesk->saveInstance(lightdesk);
  threads.insert(lightdesk->run());

  dmx->addProducer(lightdesk);

  sleep(10);

  if (State::leaving()) {
    lightdesk->leave();
  }

  State::shutdown();

  for (auto t : threads) {
    t->join();
  }

  cout << endl;
}

} // namespace pierre
