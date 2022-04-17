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
#include "rtp/event/receiver.hpp"
#include "rtsp/reply/setup.hpp"

using namespace std;
using namespace pierre;

namespace pierre {
namespace rtsp {

Setup::Setup(const Reply::Opts &opts) : Reply(opts), Aplist(requestContent()) {
  // maybe more
}

bool Setup::checksOK() const {
  auto rc_true = [](const auto &rc) { return rc == true; };

  return std::all_of(_checks.begin(), _checks.end(), rc_true);
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

  auto rc = dictGetStringArray(timing_peer_info_path, addresses_node, _timing_peer_info);

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
      constexpr auto key_timing_peer = "timingPeerInfo";
      constexpr auto key_addresses = "Addresses";
      constexpr auto key_ID = "ID";
      constexpr auto key_event_port = "eventPort";
      constexpr auto key_timing_port = "timingPort";

      Aplist reply_dict(Dictionaries{key_timing_peer});
      const auto &ip_addrs = host()->ipAddrs();

      reply_dict.dictSetStringArray(key_timing_peer, key_addresses, ip_addrs);
      reply_dict.dictSetStringVal(key_timing_peer, key_ID, ip_addrs[0]);

      auto port_barrier = rtp()->startEventReceiver();
      auto event_port = port_barrier.get(); // wait here until port is available

      reply_dict.dictSetUint(nullptr, key_event_port, event_port);
      reply_dict.dictSetUint(nullptr, key_timing_port, 0); // dummy

      size_t bytes = 0;
      auto binary = reply_dict.dictBinary(bytes);
      copyToContent(binary, bytes);

      headerAdd(ContentType, AppleBinPlist);
      responseCode(OK);

      return true;
    }
  }

  return true;
}

} // namespace rtsp
} // namespace pierre
