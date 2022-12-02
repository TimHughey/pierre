// Pierre
// Copyright (C) 2022 Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#include "reply/setup.hpp"
#include "base/host.hpp"
#include "config/config.hpp"
#include "frame/master_clock.hpp"
#include "mdns/mdns.hpp"
#include "mdns/service.hpp"
#include "reply/dict_keys.hpp"
#include "rtsp/ctx.hpp"

#include <algorithm>
#include <iterator>

namespace pierre {
namespace airplay {
namespace reply {

bool Setup::populate() {
  resp_code(RespCode::BadRequest); // default response is BadRequest

  rdict = plist();
  auto rc = rdict.ready();

  const auto has_streams = (rdict.fetchNode({dk::STREAMS}, PLIST_ARRAY)) ? true : false;

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
    resp_code(RespCode::OK);

  } else {
    INFO(moduleID(), "WARN", "implementation missing for control_type={}\n", 1);
  }

  return rc;
}

bool Setup::handleNoStreams() {
  bool rc = true;
  auto rtsp_ctx = di->rtsp_ctx;

  // deduces cat, type and timing
  rtsp_ctx->setup_stream(rdict.stringView({dk::TIMING_PROTOCOL}));

  // now we create a reply plist based on the type of stream
  if (rtsp_ctx->stream_info.is_ptp_stream()) {
    rtsp_ctx->group_id = rdict.stringView({dk::GROUP_UUID});
    rtsp_ctx->group_contains_group_leader = rdict.boolVal({dk::GROUP_LEADER});

    auto peers = rdict.stringArray({dk::TIMING_PEER_INFO, dk::ADDRESSES});
    pierre::shared::master_clock->peers(peers);

    Aplist peer_info;

    auto ip_addrs = Host().ip_addresses();
    peer_info.setArray(dk::ADDRESSES, ip_addrs);
    peer_info.setString(dk::ID, ip_addrs[0]);

    reply_dict.setArray(dk::TIMING_PEER_INFO, {peer_info});
    reply_dict.setUints({{dk::EVENT_PORT, rtsp_ctx->server_port(rtsp::EventPort)},
                         {dk::TIMING_PORT, 0}}); // unused by AP2

    // adjust Service system flags and request mDNS update
    rtsp_ctx->service->receiver_active();
    mDNS::update();

  } else if (rtsp_ctx->stream_info.is_ntp_stream()) {
    INFO(baseID(), moduleID(), "NTP timing not supported\n");
    rc = false;

  } else if (rtsp_ctx->stream_info.is_remote_control()) {
    // remote control only; open event port, send timing dummy port
    reply_dict.setUints({{dk::EVENT_PORT, rtsp_ctx->server_port(rtsp::EventPort)},
                         {dk::TIMING_PORT, 0}}); // unused by AP2
  } else {
    rc = false; // no match
  }

  return rc;
}

// SETUP followup message containing type of stream to start
bool Setup::handleStreams() {
  auto rc = false;
  auto rtsp_ctx = di->rtsp_ctx;
  auto &stream_info = rtsp_ctx->stream_info;

  Aplist reply_stream0; // reply streams sub dict

  if (stream_info.is_ptp_stream()) {
    // initial setup indicated PTP, the followup setup message finalizes details

    const auto s0 = Aplist(rdict, {dk::STREAMS, dk::IDX0});

    // capture stream info
    stream_info.audio_format = s0.uint({dk::AUDIO_MODE});
    stream_info.ct = (uint8_t)s0.uint({dk::COMPRESSION_TYPE});
    stream_info.conn_id = s0.uint({dk::STREAM_CONN_ID});
    stream_info.spf = s0.uint({dk::SAMPLE_FRAMES_PER_PACKET});

    stream_info.supports_dynamic_stream_id = s0.boolVal({dk::SUPPORTS_DYNAMIC_STREAM_ID});
    stream_info.audio_format = s0.uint({dk::AUDIO_FORMAT});
    stream_info.client_id = s0.stringView({dk::CLIENT_ID});

    rtsp_ctx->active_remote = rHeaders().val<int64_t>(hdr_type::DacpActiveRemote);
    rtsp_ctx->dacp_id = rHeaders().val(hdr_type::DacpID);
    rtsp_ctx->shared_key = s0.dataArray({dk::SHK}); // copy is ok here

    auto stream_type = rtsp_ctx->setup_stream_type(s0.uint({dk::TYPE}));

    // now handle the specific stream type
    if (stream_info.is_buffered()) {
      rc = true;
      const uint64_t buff_size = Config().at("rtsp.audio.buffer_size"sv).value_or(8) * 1024 * 1024;

      // reply requires the type, audio data port and our buffer size
      reply_stream0.setUints({{dk::TYPE, stream_type}, // stream type
                              {dk::DATA_PORT, rtsp_ctx->server_port(rtsp::AudioPort)}, // audio port
                              {dk::BUFF_SIZE, buff_size}}); // our buffer size

    } else if (stream_info.is_realtime()) {
      rc = false;
      INFO(module_id, "STREAM", "realtime not supported\n");

    } else if (stream_info.is_remote_control()) {
      rc = true;

      reply_stream0.setUints({
          {dk::STREAM_ID, 1},
          {dk::TYPE, stream_type},
      });
    }

  } else if (stream_info.is_ntp_stream()) {
    INFO(module_id, "STREAM", "ntp timing not supported\n");

  } else {
    INFO(module_id, "STREAM", "unknown / unsupported stream\n");
  }

  if (rc) {
    // regardless of stream type start the control server
    reply_stream0.setUint(dk::CONTROL_PORT, rtsp_ctx->server_port(rtsp::ControlPort));

    // put the sub dict into the reply
    reply_dict.setArray(dk::STREAMS, reply_stream0);
  }

  return rc;
}

} // namespace reply
} // namespace airplay
} // namespace pierre
