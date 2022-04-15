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
#include "rtsp/reply/xml.hpp"

using namespace std;

namespace pierre {
namespace rtsp {

constexpr auto qualifier = "txtAirPlay";
constexpr auto qualifier_len = strlen(qualifier);

Setup::Setup(sRequest request) : Reply(request), Aplist(request->content()) {}

bool Setup::checkTimingProtocol() {
  constexpr auto timing_path = "timingProtocol";
  constexpr auto ptp = "PTP";

  auto match = dictCompareString(timing_path, ptp);

  if (!match) {
    fmt::print("Setup: only {} timing allowed\n", ptp);
    return false;
  }

  _stream_category = PtpStream;
  _timing_type = TsPtp;

  // get play lock??

  return true;
}

bool Setup::getGroupInfo() {
  constexpr auto group_uuid_path = "groupUUID";
  constexpr auto gcl_path = "groupContainsGroupLeader";

  auto rc = false;
  std::vector<bool> results;

  rc = dictGetString(group_uuid_path, _group_uuid);
  results.emplace_back(rc);

  rc = dictGetBool(gcl_path, _group_contains_leader);
  results.emplace_back(rc);

  return std::any_of(results.begin(), results.end(),
                     [](auto rc) { return rc == false; });
}

bool Setup::getTimingList() {
  constexpr auto timing_peer_info_path = "timingPeerInfo";
  constexpr auto addresses_node = "Addresses";

  auto rc = dictGetStringArray(timing_peer_info_path, addresses_node,
                               _timing_peer_info);

  // fmt::print("Setup::getTimingList(): rc={} num_strings={}\n", rc,
  //            _timing_peer_info.size());

  return rc;
}

bool Setup::populate() {
  constexpr auto streams = "streams";

  if (dictReady() == false) {
    return false;
  }

  // initial setup, no streams active
  if (dictItemExists(streams) == false) {
    checkTimingProtocol();
    getGroupInfo();
    getTimingList();
  }

  return true;
}

} // namespace rtsp
} // namespace pierre
