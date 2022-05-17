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

#include "reply/setup.hpp"
#include "common/conn_info.hpp"
#include "core/service.hpp"
#include "mdns/mdns.hpp"
#include "reply/dict_keys.hpp"
#include "server/map.hpp"

#include <algorithm>
#include <chrono>
#include <fmt/core.h>
#include <fmt/format.h>
#include <iterator>
#include <memory>
#include <string>

using namespace std;
using namespace pierre;
using namespace chrono_literals;

namespace pierre {
namespace airplay {
namespace reply {

using namespace packet;
using Aplist = packet::Aplist;
using enum ServerType;
namespace header = pierre::packet::header;

bool Setup::populate() {
  responseCode(RespCode::BadRequest); // default response is BadRequest

  rdict = plist();
  auto rc = rdict.ready();

  const auto has_streams = (rdict.fetchNode({dk::STREAMS}, PLIST_ARRAY)) ? true : false;

  if (false) { // debug
    constexpr auto f = FMT_STRING("{} {} has_streams={}\n");
    fmt::print(f, runTicks(), fnName(), has_streams);
  }

  if (rc && has_streams) { // followup SETUP (contains streams data)
    rc = handleStreams();
  } else {
    rc = handleNoStreams(); // initial SETUP (streams not present)
  }

  if (rc) { // all is well, finalize the reply
    size_t bytes = 0;
    auto binary = reply_dict.toBinary(bytes);
    copyToContent(binary, bytes);

    headers.add(header::type::ContentType, header::val::AppleBinPlist);
    responseCode(RespCode::OK);
  }

  return rc;
}

bool Setup::handleNoStreams() {
  // deduces cat, type and timing
  auto stream = Stream(rdict.getView(dk::TIMING_PROTOCOL));

  // now we create a replu plist based on the type of stream
  if (stream.isPtpStream()) {
    ConnInfo &ci = conn();

    ci.airplay_gid = rdict.getView(dk::GROUP_UUID);
    ci.groupContainsGroupLeader = rdict.boolVal(dk::GROUP_LEADER);

    auto peers = rdict.stringArray({dk::TIMING_PEER_INFO, dk::ADDRESSES});
    anchor().peers(peers);

    Aplist peer_info;

    auto ip_addrs = Host::ptr()->ipAddrs();
    peer_info.setArray(dk::ADDRESSES, ip_addrs);
    peer_info.setString(dk::ID, ip_addrs[0]);

    reply_dict.setArray(dk::TIMING_PEER_INFO, {peer_info});

    reply_dict.setUints({{dk::EVENT_PORT, Servers::ptr()->localPort(Event)},
                         {dk::TIMING_PORT, 0}}); // unused by AP2

    // adjust Service system flags and request mDNS update
    Service::ptr()->deviceSupportsRelay();
    mDNS::ptr()->update();

    std::this_thread::sleep_for(100ms);

    ci.save(stream);
    return true;
  }

  if (stream.isNtpStream()) {
    constexpr auto f = FMT_STRING("{} {} WARNING NTP timing requested!");
    fmt::print(f, runTicks(), fnName());
    return false;
  }

  if (stream.isRemote()) { // remote control only; open event port, send timing dummy port
    reply_dict.setUints({{dk::EVENT_PORT, Servers::ptr()->localPort(Event)},
                         {dk::TIMING_PORT, 0}}); // unused by AP2

    return true;
  }

  // something is wrong if we reach this point
  return false;
}

// SETUP followup message containing type of stream to start
bool Setup::handleStreams() {
  // rdict.dump();

  using enum ServerType;

  auto servers = Servers::ptr();

  auto rc = false;

  auto &conn = di->conn;      // conn info for the established session
  auto &stream = conn.stream; // populated based on request dictionary

  Aplist reply_stream0; // build the streams sub dict

  if (stream.isPtpStream()) { // initial setup PTP, this setup finalizes details
    // capture stream info common for buffered or real-time
    stream_data.key = rdict.getView({dk::STREAMS, dk::IDX0, dk::SHK});
    stream_data.active_remote = rHeaders().getVal(header::type::DacpActiveRemote);
    stream_data.dacp_id = rHeaders().getVal(header::type::DacpID);

    // get the stream type that is starting
    auto stream_type = rdict.uint({dk::STREAMS, dk::IDX0, dk::TYPE});

    if (false) { // debug
      constexpr auto f = FMT_STRING("{} {} stream_type={}\n");
      fmt::print(f, runTicks(), fnName(), stream_type);
    }

    // now handle the specific stream type
    switch (stream_type) {
      case stream::typeBuffered():
        stream.buffered(); // this is a buffered audio stream

        // reply requires the type, audio data port and our buffer size
        reply_stream0.setUints({{dk::TYPE, stream::typeBuffered()},         // stream type
                                {dk::DATA_PORT, servers->localPort(Audio)}, // audio port
                                {dk::BUFF_SIZE, conn.bufferSize()}});       // our buffer size

        rc = true;
        break;

      case stream::typeRealTime():
        stream.realtime();
        rc = true;
        break;

      default:
        rc = false;
        break;
    }

  } else if (conn.stream.isRemote()) {
    reply_stream0.setUints({{
        {dk::STREAM_ID, 1},
        {dk::TYPE, conn.stream.typeVal()},
    }});
    rc = true;
  }

  if (rc) {
    // regardless of stream type start the control server and add it's port to the reply
    reply_stream0.setUint(dk::CONTROL_PORT, servers->localPort(Control));

    // put the sub dict into the reply
    reply_dict.setArray(dk::STREAMS, reply_stream0);
  }

  return rc;
}

} // namespace reply
} // namespace airplay
} // namespace pierre

/*
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

    Aplist reply_dict(Dictionaries{key_timing_peer});
    const auto &ip_addrs = host().ipAddrs();

    reply_dict.setArray(key_timing_peer, key_addresses, ip_addrs);
    reply_dict.setStringVal(key_timing_peer, key_ID, ip_addrs[0]);

    // rtp-localPort() returns the local port (and starts the service if needed)
    reply_dict.setUint(nullptr, key_event_port, conn().localPort(Event));
    reply_dict.setUint(nullptr, key_timing_port, 0); // dummy

    // adjust Service system flags and request mDNS update
    service().deviceSupportsRelay();
    mdns().update();

    size_t bytes = 0;
    auto binary = reply_dict.toBinary(bytes);
    copyToContent(binary, bytes);

    headers.add(header::type::ContentType, header::val::AppleBinPlist);
    responseCode(RespCode::OK);

    rc = true;
  }

  return rc;
}

*/

/*
bool Setup::__handleStreams() {
  constexpr auto PTP = 103;

  auto rc = true;

  // save session info
  // path is streams -> array[0]
  auto rstream0 = Aplist(rdict, Level::Second, dictKey(streams), 0);

  conn().saveStreamData(
      {.audio_mode = rstream0.getStringConst(Root, dictKey(audioMode)),
       .ct = (uint8_t)rstream0.getUint(Root, dictKey(ct)),
       .conn_id = rstream0.getUint(Root, dictKey(streamConnectionID)),
       .spf = (uint32_t)rstream0.getUint(Root, dictKey(spf)),
       .key = rstream0.getData(Root, dictKey(shk)),
       .supports_dynamic_stream_id = rstream0.getBool(Root, dictKey(supportsDyanamicStreamID)),
       .audio_format = (uint32_t)rstream0.getUint(Root, dictKey(audioFormat)),
       .client_id = rstream0.getStringConst(Root, dictKey(clientID)),
       .type = (uint8_t)rstream0.getUint(Root, dictKey(type)),
       .active_remote = rHeaders().getVal(header::type::DacpActiveRemote),
       .dacp_id = rHeaders().getVal(header::type::DacpID)});

  conn().saveSessionKey(rstream0.getData(Root, dictKey(shk)));
  conn().saveActiveRemote(rHeaders().getVal(header::type::DacpActiveRemote));

  // build the reply (includes ports for started services)
  Aplist::ArrayDicts array_dicts;
  auto &stream0_dict = array_dicts.emplace_back(Aplist());

  // rtp->localPort() returns the local port (and starts the service if needed)
  stream0_dict.setUint(nullptr, dictKey(controlPort), conn().localPort(Control));
  stream0_dict.setUint(nullptr, dictKey(type), PTP);
  stream0_dict.setUint(nullptr, dictKey(dataPort), conn().localPort(Audio));
  stream0_dict.setUint(nullptr, dictKey(audioBufferSize), conn().bufferSize());

  Aplist reply_dict;

  reply_dict.setArray(dictKey(streams), array_dicts);

  size_t bytes = 0;
  auto binary = reply_dict.toBinary(bytes);
  copyToContent(binary, bytes);

  headers.add(header::type::ContentType, header::val::AppleBinPlist);
  responseCode(RespCode::OK);

  return rc;
}
*/

/*

void Setup::getGroupInfo() {
  constexpr auto group_uuid_path = "groupUUID";
  constexpr auto gcl_path = "groupContainsGroupLeader";

  // add the result of getString to the checks vector
  saveCheck(rdict.getString(group_uuid_path, _group_uuid));
  saveCheck(rdict.getBool(gcl_path, _group_contains_leader));
}

void Setup::getTimingList() {
  constexpr auto timing_peer_info_path = "timingPeerInfo";
  constexpr auto addresses_node = "Addresses";

  auto rc = rdict.getStringArray(timing_peer_info_path, addresses_node, _timing_peer_info);

  if (rc) {
    auto &anchor = injected().anchor;

    anchor.peers(_timing_peer_info);
  }

  saveCheck(rc);
}

void Setup::validateTimingProtocol() {
  auto match = rdict.compareString(dictKey(timingProtocol), timingProtocolVal(PreciseTiming));

  if (!match) {
    const auto prefix = fmt::format("{} unhandled timing protocol={}", fnName(),
                                    rdict.getStringConst(Root, dictKey(timingProtocol)));
    rdict.dump(prefix.c_str());
    saveCheck(false);
    return;
  }

  // get play lock??

  saveCheck(true);
}

*/

// static maps
/*
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
                                                     {PreciseTiming, "PTP"}};
*/
