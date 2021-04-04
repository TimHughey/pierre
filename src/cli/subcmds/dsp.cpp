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

#include <boost/format.hpp>
#include <boost/tokenizer.hpp>
#include <iostream>

#include "audio/dsp.hpp"
#include "cli/subcmds/dsp.hpp"

using namespace std;
using boost::format;
using boost::tokenizer;
using namespace pierre::audio;

namespace pierre {
namespace cli {

int Dsp::handleCmd(const string &args) {
  auto rc = 0;

  tokens.clear();

  tokenizer<> t(args);
  for (tokenizer<>::iterator x = t.begin(); x != t.end(); ++x) {
    string token(*x);
    tokens.push_back(std::move(token));
  }

  auto tok = tokens.cbegin();

  if (tok->compare("help") == 0) {
    cout << "dsp command help:" << endl;
    cout << "config          - show configuration" << endl;
    cout << "scale <action>  - mag scale display and control" << endl;
    cout << "                  action=<increase|reduce|reset>" << endl;
    goto finished;
  }

  if (tok->compare("scale") == 0) {
    tokens.pop_front();
    rc = handleScale();
    goto finished;
  }

  if (tok->compare("config") == 0) {
    auto config = Peak::config();

    cout << "magnitude floor=" << config.floor() << " ";
    cout << "strong_multiplier=" << config.strong() << " ";
    cout << "ceiling=" << config.ceiling() << endl;
    goto finished;
  }

  rc = -1;

finished:
  return rc;
}

int Dsp::handleScale() {
  int rc = 0;

  auto tok = tokens.cbegin(); // skip mag

  if (tok >= tokens.cend()) {
    printScale();
    goto finished;
  }

  if (tok->compare("reduce") == 0) {
    auto &config = Peak::config();
    config.scaleReduce();
    printScale();

    goto finished;
  }

  if (tok->compare("reset") == 0) {
    auto &config = Peak::config();
    config.reset();
    printScale();

    goto finished;
  }

  if (tok->compare("increase") == 0) {
    auto &config = Peak::config();
    config.scaleIncrease();
    printScale();

    goto finished;
  }

finished:

  return rc;
}

void Dsp::printScale() {
  auto &cfg = Peak::config();
  const auto &scale = cfg.activeScale();

  cout << boost::format(
              "scale: floor[%8.3f] ceiling[%8.3f] factor[%5.2f] step[%5.3f]") %
              scale.min() % scale.max() % cfg.scaleFactor() % cfg.step();
}

} // namespace cli
} // namespace pierre
