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
#include "base/host.hpp"
#include "conn_info/conn_info.hpp"
#include "core/service.hpp"
#include "frame/anchor.hpp"
#include "frame/master_clock.hpp"
#include "mdns/mdns.hpp"
#include "reply/dict_keys.hpp"
#include "server/servers.hpp"

#include <algorithm>
#include <iterator>

namespace pierre {
namespace airplay {
namespace reply {

bool Setup::populate() {
  responseCode(RespCode::BadRequest); // default response is BadRequest

  rdict = plist();
  auto rc = rdict.ready();

  // rdict.dump();

  const auto has_streams = (rdict.fetchNode({dk::STREAMS}, PLIST_ARRAY)) ? true : false;

  INFOX(baseID(), moduleID(), "has_streams={}\n", has_streams);

  if (rc && has_streams) { // followup SETUP (contains streams data)
    rc = handleStreams();
  } else {
    rc = handleNoStreams(); // initial SETUP (streams not present)
  }

  if (rc) { // all is well, finalize the reply
    size_t bytes = 0;
    auto binary = reply_dict.toBinary(bytes);
    copyToContent(binary, bytes);

    headers.add(hdr_type::ContentType, hdr_val::AppleBinPlist);
    responseCode(RespCode::OK);
  } else {
    INFO(moduleID(), "WARN", "implementation missing for control_type={}\n", 1);
    // rHeaders().dump();
    // rdict.dump();
  }

  return rc;
}

bool Setup::handleNoStreams() {
  // auto &anchor = *pierre::shared::anchor;
  auto conn = ConnInfo::ptr();

  // deduces cat, type and timing
  auto stream = Stream(rdict.stringView({dk::TIMING_PROTOCOL}));

  // now we create a reply plist based on the type of stream
  if (stream.isPtpStream()) {
    conn->airplay_gid = rdict.stringView({dk::GROUP_UUID});
    conn->groupContainsGroupLeader = rdict.boolVal({dk::GROUP_LEADER});

    auto peers = rdict.stringArray({dk::TIMING_PEER_INFO, dk::ADDRESSES});
    pierre::shared::master_clock->peers(peers);

    Aplist peer_info;

    auto ip_addrs = Host::ptr()->ipAddrs();
    peer_info.setArray(dk::ADDRESSES, ip_addrs);
    peer_info.setString(dk::ID, ip_addrs[0]);

    reply_dict.setArray(dk::TIMING_PEER_INFO, {peer_info});
    reply_dict.setUints({{dk::EVENT_PORT, Servers::ptr()->localPort(ServerType::Event)},
                         {dk::TIMING_PORT, 0}}); // unused by AP2

    // adjust Service system flags and request mDNS update
    Service::ptr()->receiverActive();
    mDNS::update();

    conn->save(stream);
    return true;
  }

  if (stream.isNtpStream()) {
    INFO(baseID(), moduleID(), "NTP timing requeste, supported={}\n", false);
    return false;
  }

  if (stream.isRemote()) { // remote control only; open event port, send timing
                           // dummy port
    reply_dict.setUints({{dk::EVENT_PORT, Servers::ptr()->localPort(ServerType::Event)},
                         {dk::TIMING_PORT, 0}}); // unused by AP2

    return true;
  }

  // something is wrong if we reach this point
  return false;
}

// SETUP followup message containing type of stream to start
bool Setup::handleStreams() {
  auto servers = Servers::ptr();
  auto conn = ConnInfo::ptr();

  auto rc = false;
  StreamData stream_data;
  Aplist reply_stream0; // build the streams sub dict

  if (conn->stream.isPtpStream()) { // initial setup is PTP, this setup
                                    // finalizes details
    const auto s0 = Aplist(rdict, {dk::STREAMS, dk::IDX0});

    // capture stream info common for buffered or real-time
    stream_data.audio_format = s0.uint({dk::AUDIO_MODE});
    stream_data.ct = (uint8_t)s0.uint({dk::COMPRESSION_TYPE});
    stream_data.conn_id = s0.uint({dk::STREAM_CONN_ID});
    stream_data.spf = s0.uint({dk::SAMPLE_FRAMES_PER_PACKET});
    stream_data.key = s0.dataArray({dk::SHK});
    stream_data.supports_dynamic_stream_id = s0.boolVal({dk::SUPPORTS_DYNAMIC_STREAM_ID});
    stream_data.audio_format = s0.uint({dk::AUDIO_FORMAT});
    stream_data.client_id = s0.stringView({dk::CLIENT_ID});
    stream_data.type = s0.uint({dk::TYPE});

    stream_data.active_remote = rHeaders().getVal(hdr_type::DacpActiveRemote);
    stream_data.dacp_id = rHeaders().getVal(hdr_type::DacpID);

    ConnInfo::ptr()->save(stream_data);

    // get the stream type that is starting
    auto stream_type = rdict.uint({dk::STREAMS, dk::IDX0, dk::TYPE});

    INFOX(baseID(), moduleID(), "stream_type={}\n", stream_type);

    // now handle the specific stream type
    switch (stream_type) {
    case stream::typeBuffered():
      conn->stream.buffered(); // this is a buffered audio stream

      // reply requires the type, audio data port and our buffer size
      reply_stream0.setUints({{dk::TYPE, stream::typeBuffered()},                     // stream type
                              {dk::DATA_PORT, servers->localPort(ServerType::Audio)}, // audio port
                              {dk::BUFF_SIZE, conn->bufferSize()}}); // our buffer size

      rc = true;
      break;

    case stream::typeRealTime():
      conn->stream.realtime();
      rc = true;
      break;

    default:
      rc = false;
      break;
    }

  } else if (conn->stream.isRemote()) {
    reply_stream0.setUints({{
        {dk::STREAM_ID, 1},
        {dk::TYPE, conn->stream.typeVal()},
    }});
    rc = true;
  }

  if (rc) {
    // regardless of stream type start the control server and add it's port to
    // the reply
    reply_stream0.setUint(dk::CONTROL_PORT, servers->localPort(ServerType::Control));

    // put the sub dict into the reply
    reply_dict.setArray(dk::STREAMS, reply_stream0);
  }

  return rc;
}

} // namespace reply
} // namespace airplay
} // namespace pierre
