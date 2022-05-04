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
#include "decouple/conn_info.hpp"
#include "decouple/stream_info.hpp"
#include "rtp/rtp.hpp"
#include "rtsp/reply/setup.hpp"

using namespace std;
using namespace pierre;

namespace pierre {
namespace rtsp {
using namespace packet;

using enum rtp::ServerType;

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
    service()->deviceSupportsRelay();
    mdns()->update();

    size_t bytes = 0;
    auto binary = reply_dict.dictBinary(bytes);
    copyToContent(binary, bytes);

    headers.add(Headers::Type2::ContentType, Headers::Val2::AppleBinPlist);
    responseCode(RespCode::OK);

    // reply_dict.dictDump();

    rc = true;
  }

  return rc;
}

bool Setup::handleStreams() {
  auto conn_info = ConnInfo::inst();
  constexpr auto PTP = 103;

  using enum Headers::Type2;
  auto rc = true;

  // RTP is a singleton, grab the shared_ptr (save some typing)
  auto rtp = Rtp::instance();

  // save session info
  // path is streams -> array[0]
  auto rstream0 = Aplist(dictSelf(), Level::Second, dictKey(streams), 0);

  rtp->save(
      {.audio_mode = rstream0.dictGetStringConst(Root, dictKey(audioMode)),
       .ct = (uint8_t)rstream0.dictGetUint(Root, dictKey(ct)),
       .conn_id = rstream0.dictGetUint(Root, dictKey(streamConnectionID)),
       .spf = (uint32_t)rstream0.dictGetUint(Root, dictKey(spf)),
       .key = rstream0.dictGetData(Root, dictKey(shk)),
       .supports_dynamic_stream_id = rstream0.dictGetBool(Root, dictKey(supportsDyanamicStreamID)),
       .audio_format = (uint32_t)rstream0.dictGetUint(Root, dictKey(audioFormat)),
       .client_id = rstream0.dictGetStringConst(Root, dictKey(clientID)),
       .type = (uint8_t)rstream0.dictGetUint(Root, dictKey(type)),
       .active_remote = rHeaders().getVal(DacpActiveRemote),
       .dacp_id = rHeaders().getVal(DacpID)});

  conn_info->saveSessionKey(rstream0.dictGetData(Root, dictKey(shk)));

  // build the reply (includes portS for started services)
  ArrayDicts array_dicts;
  auto &stream0_dict = array_dicts.emplace_back(Aplist());

  // rtp->localPort() returns the local port (and starts the service if needed)
  stream0_dict.dictSetUint(nullptr, dictKey(controlPort), rtp->localPort(Control));
  stream0_dict.dictSetUint(nullptr, dictKey(type), PTP);
  stream0_dict.dictSetUint(nullptr, dictKey(dataPort), rtp->localPort(Audio));
  stream0_dict.dictSetUint(nullptr, dictKey(audioBufferSize), rtp->bufferSize());

  Aplist reply_dict;

  reply_dict.dictSetArray(dictKey(streams), array_dicts);

  size_t bytes = 0;
  auto binary = reply_dict.dictBinary(bytes);
  copyToContent(binary, bytes);

  headers.add(Headers::Type2::ContentType, Headers::Val2::AppleBinPlist);
  responseCode(RespCode::OK);

  // reply_dict.dictDump();

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
  constexpr auto remote_control = "isRemoteControlOnly";

  if (dictReady() == false) {
    return false;
  }

  // handle RemoteControlOnly (not supported, but still RespCode=OK)
  if (dictItemExists(remote_control)) {
    auto fmt_str = FMT_STRING("{} isRemoteControlOnly=true\n");
    fmt::print(fmt_str, fnName());
    return true;
  }

  // initial setup, no streams active
  if (dictItemExists(streams) == false) {
    return handleNoStreams();
  }

  if (dictItemExists(streams)) {
    return handleStreams();
  }

  return rc;
}

void Setup::validateTimingProtocol() {
  auto match = dictCompareString(dictKey(timingProtocol), timingProtocolVal(PreciseTiming));

  if (!match) {
    const auto prefix = fmt::format("{} unhandled timing protocol={}", fnName(),
                                    dictGetStringConst(Root, dictKey(timingProtocol)));
    dictDump(prefix.c_str());
    saveCheck(false);
    return;
  }

  // get play lock??

  saveCheck(true);
}

// static maps

using enum Setup::DictKey;
Setup::DictKeyMap Setup::_key_map{{audioMode, "audioMode"},
                                  {ct, "ct"},
                                  {streamConnectionID, "streamConnectionID"},
                                  {spf, "spf"},
                                  {shk, "shk"},
                                  {supportsDyanamicStreamID, "supportsDynamicStreamID"},
                                  {audioFormat, "audioFormat"},
                                  {clientID, "clientID"},
                                  {type, "type"},
                                  {controlPort, "controlPort"},
                                  {dataPort, "dataPort"},
                                  {audioBufferSize, "audioBufferSize"},
                                  {streams, "streams"},
                                  {timingProtocol, "timingProtocol"}};

Setup::StreamTypeMap Setup::_stream_type_map{
    {StreamType::Unknown, 0}, {StreamType::Buffered, 103}, {StreamType::RealTime, 96}};

Setup::TimingProtocolMap Setup::_timing_protocol_map{{TimingProtocol::None, "None"},
                                                     {PreciseTiming, "PTP"}

};
} // namespace rtsp
} // namespace pierre
