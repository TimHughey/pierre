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
#include "rtp/rtp.hpp"
#include "rtsp/reply/setup.hpp"

using namespace std;
using namespace pierre;

namespace pierre {
namespace rtsp {
using namespace packet;

using enum ServerType;

Setup::Setup(const Reply::Opts &opts) : Reply(opts), Aplist(rContent()) {
  // dictDump();
  // maybe more
  debugFlag(false);
}

bool Setup::checksOK() const {
  auto rc_true = [](const auto &rc) { return rc == true; };

  return std::all_of(_checks.begin(), _checks.end(), rc_true);
}

bool Setup::handleNoStreams() {
  auto rc = false;
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

    // RTP is a singleton, grab the shared_ptr (save some typing)
    auto rtp = Rtp::instance();

    Aplist reply_dict(Dictionaries{key_timing_peer});
    const auto &ip_addrs = host()->ipAddrs();

    reply_dict.dictSetStringArray(key_timing_peer, key_addresses, ip_addrs);
    reply_dict.dictSetStringVal(key_timing_peer, key_ID, ip_addrs[0]);

    // rtp-localPort() returns the local port (and starts the service if needed)
    reply_dict.dictSetUint(nullptr, key_event_port, rtp->localPort(Event));
    reply_dict.dictSetUint(nullptr, key_timing_port, 0); // dummy

    // adjust Service system flags and request mDNS update
    service()->adjustSystemFlags(Service::Flags::DeviceSupportsRelay);
    mdns()->update();

    size_t bytes = 0;
    auto binary = reply_dict.dictBinary(bytes);
    copyToContent(binary, bytes);

    headers.add(Headers::Type2::ContentType, Headers::Val2::AppleBinPlist);
    responseCode(RespCode::OK);

    rc = true;
  }

  return rc;
}

bool Setup::handleStreams() {
  constexpr auto PTP = 103;
  constexpr auto key_shared_key = "shk";
  constexpr auto key_control_port = "controlPort";
  constexpr auto key_type = "type";
  constexpr auto key_data_port = "dataPort";
  constexpr auto key_audio_buff_size = "audioBufferSize";
  constexpr auto key_streams = "streams";
  using enum Headers::Type2;
  auto rc = true;

  // get session info
  const auto session_key = dictGetData(1, key_shared_key);
  const auto active_remote = rHeaders().getVal(DacpActiveRemote);
  const auto dacp_id = rHeaders().getVal(DacpID);

  // RTP is a singleton, grab the shared_ptr (save some typing)
  auto rtp = Rtp::instance();

  rtp->saveSessionInfo(session_key, active_remote, dacp_id);

  // build the reply (includes port for started services)
  ArrayDicts array_dicts;
  auto &stream0_dict = array_dicts.emplace_back(Aplist());

  // rtp->localPort() returns the local port (and starts the service if needed)
  stream0_dict.dictSetUint(nullptr, key_control_port, rtp->localPort(Control));
  stream0_dict.dictSetUint(nullptr, key_type, PTP);
  stream0_dict.dictSetUint(nullptr, key_data_port, rtp->localPort(AudioBuffered));
  stream0_dict.dictSetUint(nullptr, key_audio_buff_size, rtp->bufferSize());

  Aplist reply_dict;

  reply_dict.dictSetArray(key_streams, array_dicts);

  size_t bytes = 0;
  auto binary = reply_dict.dictBinary(bytes);
  copyToContent(binary, bytes);

  headers.add(Headers::Type2::ContentType, Headers::Val2::AppleBinPlist);
  responseCode(RespCode::OK);

  return rc;
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

  nptp()->sendTimingPeers(_timing_peer_info);

  saveCheck(rc);
}

bool Setup::populate() {
  auto rc = false;
  constexpr auto streams = "streams";

  if (dictReady() == false) {
    return false;
  }

  // initial setup, no streams active
  if (dictItemExists(streams) == false) {
    rc = handleNoStreams();
  } else {
    rc = handleStreams();
  }

  return rc;
}

void Setup::validateTimingProtocol() {
  constexpr auto timing_path = "timingProtocol";
  constexpr auto ptp = "PTP";

  auto match = dictCompareString(timing_path, ptp);

  if (!match) {
    const auto prefix = fmt::format("{} timing type != {}", fnName(), ptp);
    dictDump(prefix.c_str());
    saveCheck(false);
    return;
  }

  _stream_category = PtpStream;
  _timing_type = TsPtp;

  // get play lock??

  saveCheck(true);
}

} // namespace rtsp
} // namespace pierre
