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
#include "pierre.hpp"

namespace pierre {

using namespace std;
using namespace audio;
using namespace lightdesk;

using namespace boost::asio;

Pierre::Pierre(const Config &cfg) : _cfg(cfg) {}

void Pierre::run() {
  std::unordered_set<std::shared_ptr<std::thread>> threads;

  pcm = make_shared<Pcm>(_cfg.pcm);
  threads.insert(pcm->run());

  dsp = make_shared<Dsp>(_cfg.dsp);
  threads.insert(dsp->run());

  dmx = make_shared<dmx::Render>(_cfg.dmx);
  threads.insert(dmx->run());

  lightdesk = make_unique<LightDesk>(_cfg.lightdesk, dsp);
  threads.insert(lightdesk->run());

  pcm->addProcessor(dsp);
  dmx->addProducer(lightdesk);

  Cli cli;
  cli.run();

  lightdesk->shutdown();
  dmx->shutdown();
  dsp->shutdown();
  pcm->shutdown();

  for (auto t : threads) {
    t->join();
  }
}
} // namespace pierre
