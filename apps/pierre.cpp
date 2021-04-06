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

#include <thread>
#include <unordered_set>

#include "audio/net.hpp"
#include "audio/pcm.hpp"
#include "core/state.hpp"
#include "pierre.hpp"

namespace pierre {

using namespace std;
using namespace audio;
using namespace lightdesk;
using namespace core;

using namespace boost::asio;

Pierre::Pierre(core::Config &cfg) : _cfg(cfg) {}

void Pierre::run() {

  std::unordered_set<std::shared_ptr<std::thread>> threads;

  pcm = make_shared<Pcm>(_cfg);
  threads.insert(pcm->run());

  dsp = make_shared<Dsp>(_cfg);
  threads.insert(dsp->run());

  dmx = make_shared<dmx::Render>(_cfg);
  threads.insert(dmx->run());

  lightdesk = make_shared<LightDesk>(_cfg, dsp);
  lightdesk->saveInstance(lightdesk);
  threads.insert(lightdesk->run());

  pcm->addProcessor(dsp);
  dmx->addProducer(lightdesk);

  Cli cli(_cfg);
  cli.run();

  if (State::leaving()) {
    lightdesk->leave();
  }

  State::shutdown();

  for (auto t : threads) {
    t->join();
  }

  cout << endl;
}

// initialize State singleton
core::State core::State::i = core::State();
} // namespace pierre
