/*
    Pierre - Custom Light Show for Wiss Landing
    Copyright (C) 2022  Tim Hughey

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

#include <algorithm>
#include <fmt/core.h>
#include <fmt/format.h>
#include <iterator>
#include <memory>
#include <string>

#include "nptp/nptp.hpp"
#include "rtsp/reply/set_peers.hpp"

using namespace std;
using namespace pierre;

namespace pierre {
namespace rtsp {

SetPeers::SetPeers(const Reply::Opts &opts) : Reply(opts), Aplist(requestContent()) {
  // dictDump();
  // maybe more
  debugFlag(false);
}

bool SetPeers::populate() {
  Aplist::ArrayStrings timing_peers;

  auto rc = dictGetStringArray(nullptr, nullptr, timing_peers);

  if (rc) {
    nptp()->sendTimingPeers(timing_peers);

    responseCode(OK);

    return true;
  }

  return false;
}

} // namespace rtsp
} // namespace pierre
