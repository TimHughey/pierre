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

#include "audio/dsp.hpp"
#include "cli/subcmds/dsp.hpp"

using namespace std;
using namespace pierre::audio;

namespace pierre {
namespace cli {

int Dsp::handleCmd(const string_t &args) {
  auto rc = 0;

  if (args.compare("help") == 0) {
    cout << "dsp command help:" << endl;
    cout << "config     - show configuration" << endl;
    cout << "scale      - show magnitude scale configuration" << endl;
    goto finished;
  }

  if (args.compare("config") == 0) {
    auto config = Peak::config();

    cout << "magnitude floor=" << config.floor() << " ";
    cout << "strong_multiplier=" << config.strong() << " ";
    cout << "ceiling=" << config.ceiling() << endl;
    goto finished;
  }

  if (args.compare("scale") == 0) {
    auto scale = Peak::scale();

    cout << "magnitude scale min=" << scale.min << " ";
    cout << "max=" << scale.max << endl;
    goto finished;
  }

finished:
  return rc;
}

} // namespace cli
} // namespace pierre
