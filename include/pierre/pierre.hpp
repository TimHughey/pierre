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

#ifndef _pierre_pierre_hpp
#define _pierre_pierre_hpp

#include <thread>

#include "audio/dsp.hpp"
#include "audio/net.hpp"
#include "audio/pcm.hpp"
#include "cli/cli.hpp"
#include "dmx/render.hpp"
#include "lightdesk/lightdesk.hpp"

namespace pierre {

class Pierre {
public:
  struct Config {
    audio::Pcm::Config pcm;
    audio::Dsp::Config dsp;
    lightdesk::LightDesk::Config lightdesk;
    dmx::Render::Config dmx;
  };

public:
  Pierre() = delete;
  Pierre(const Config &cfg);

  Pierre(const Pierre &) = delete;
  Pierre &operator=(const Pierre &) = delete;

  void run();

private:
  Config _cfg;

  std::shared_ptr<audio::Pcm> pcm;
  std::shared_ptr<audio::Dsp> dsp;
  std::shared_ptr<lightdesk::LightDesk> lightdesk;
  std::shared_ptr<dmx::Render> dmx;
};

} // namespace pierre

#endif
