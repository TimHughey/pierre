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

#include "core/service.hpp"
#include "rtsp/reply/setup.hpp"

using namespace std;

namespace pierre {
namespace rtsp {

Setup::Setup(const Reply::Opts &opts) : Reply(opts), Aplist(requestContent()) {
  // maybe more
}

bool Setup::checksOK() const {
  auto rc_false = [](const auto rc) { return rc == false; };

  return std::any_of(_checks.begin(), _checks.end(), rc_false);
}

void Setup::validateTimingProtocol() {
  constexpr auto timing_path = "timingProtocol";
  constexpr auto ptp = "PTP";

  auto match = dictCompareString(timing_path, ptp);

  if (!match) {
    fmt::print("Setup: only {} timing allowed\n", ptp);
    saveCheck(false);
    return;
  }

  _stream_category = PtpStream;
  _timing_type = TsPtp;

  // get play lock??

  saveCheck(true);
}

void Setup::getGroupInfo() {
  constexpr auto group_uuid_path = "groupUUID";
  constexpr auto gcl_path = "groupContainsGroupLeader";

  // add the result of dictGetString to the checks vector
  saveCheck(dictGetString(group_uuid_path, _group_uuid));

  saveCheck(dictGetBool(gcl_path, _group_contains_leader));
}

void Setup::getTimingList() {
  constexpr auto timing_peer_info_path = "timingPeerInfo";
  constexpr auto addresses_node = "Addresses";

  auto rc = dictGetStringArray(timing_peer_info_path, addresses_node,
                               _timing_peer_info);

  // fmt::print("Setup::getTimingList(): rc={} num_strings={}\n", rc,
  //            _timing_peer_info.size());

  saveCheck(rc);
}

bool Setup::populate() {
  constexpr auto streams = "streams";

  if (dictReady() == false) {
    return false;
  }

  // initial setup, no streams active
  if (dictItemExists(streams) == false) {
    std::vector<bool> checks;

    validateTimingProtocol();
    getGroupInfo();
    getTimingList();

    if (checksOK()) {
      Aplist reply_dict(Dictionaries{"timingPeerInfo"});
    }
  }

  return true;
}

} // namespace rtsp
} // namespace pierre
