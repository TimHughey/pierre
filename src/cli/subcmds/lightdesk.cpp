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
#include <iomanip>
#include <iostream>

#include "cli/subcmds/lightdesk.hpp"
#include "lightdesk/lightdesk.hpp"

using namespace std;
using boost::format;
using boost::tokenizer;
using namespace pierre::lightdesk;

namespace pierre {
namespace cli {

int LightDesk::handleCmd(const string_t &args) {
  auto rc = 0;

  tokens.clear();

  tokenizer<> t(args);
  for (tokenizer<>::iterator x = t.begin(); x != t.end(); ++x) {
    string_t token(*x);
    tokens.push_back(std::move(token));
  }

  auto tok = tokens.cbegin();

  if (tok->compare("help") == 0) {
    cout << "desk command help:" << endl;
    cout << "histo           - show MajorPeak histogram" << endl;
    goto finished;
  }

  if (tok->compare("histo") == 0) {
    tokens.pop_front();
    rc = handleHisto();
    goto finished;
  }

finished:
  return rc;
}

int LightDesk::handleHisto() {
  auto rc = 0;

  auto desk = lightdesk::LightDesk::desk();
  auto fx = desk->activeFx();
  auto histo = fx->histogram();

  auto sum_of_bins = 0.0;

  for (auto &x : histo) {
    auto width = 0;

    if (x.first < 100) {
      width = 5;
    } else if ((x.first >= 100) && (x.first < 1000)) {
      width = 5;
    } else if ((x.first >= 1000) && (x.first < 10000)) {
      width = 5;
    } else if (x.first >= 10000) {
      width = 6;
    }

    cout << right << setw(width) << x.first;

    sum_of_bins += x.second;
  }

  cout << endl;

  for (auto &x : histo) {
    auto width = 0;

    if (x.first < 100) {
      width = 5;
    } else if ((x.first >= 100) && (x.first < 1000)) {
      width = 5;
    } else if ((x.first >= 1000) && (x.first < 10000)) {
      width = 5;
    } else if (x.first >= 10000) {
      width = 6;
    }

    uint ratio = (x.second / sum_of_bins) * 100;

    cout << right << setw(width) << ratio;
  }

  cout << endl;

  return rc;
}

// void LightDesk::printScale() {
//   auto &cfg = Peak::config();
//   auto scale = cfg.activeScale();
//
//   cout << boost::format(
//               "scale: floor[%8.3f] ceiling[%8.3f] factor[%5.2f] step[%5.3f]")
//               % scale->min() % scale->max() % cfg.scaleFactor() % cfg.step();
// }

} // namespace cli
} // namespace pierre
