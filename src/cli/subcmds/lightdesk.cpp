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

int LightDesk::handleCmd(const string &args) {
  auto rc = 0;

  tokens.clear();

  tokenizer<> t(args);
  for (tokenizer<>::iterator x = t.begin(); x != t.end(); ++x) {
    string token(*x);
    tokens.push_back(std::move(token));
  }

  auto tok = tokens.cbegin();

  if (tok->compare("help") == 0) {
    cout << "desk command help:" << endl;

    goto finished;
  }

finished:
  return rc;
}

} // namespace cli
} // namespace pierre
