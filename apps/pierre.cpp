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

#include "audio/dsp.hpp"
#include "audio/pcm.hpp"
#include "cli/cli.hpp"
#include "dmx/render.hpp"
#include "lightdesk/lightdesk.hpp"
#include "pierre.hpp"

namespace pierre
{
  using namespace std;

  void Pierre::run()
  {
    unordered_set<shared_ptr<thread>> threads;

    auto pcm = make_shared<audio::Pcm>();
    auto dsp = make_shared<audio::Dsp>();
    auto dmx = make_shared<dmx::Render>();
    auto lightdesk = make_shared<lightdesk::LightDesk>(dsp);

    threads.insert(pcm->run());
    threads.insert(dsp->run());
    threads.insert(dmx->run());

    lightdesk->saveInstance(lightdesk);
    threads.insert(lightdesk->run());

    pcm->addProcessor(dsp);
    dmx->addProducer(lightdesk);

    Cli cmdLine = Cli();
    cmdLine.run();

    if (State::leaving())
    {
      lightdesk->leave();
    }

    State::shutdown();

    for (auto t : threads)
    {
      t->join();
    }

    cout << endl;
  }

} // namespace pierre
