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

#include "replies/setup.hpp"
#include "base/host.hpp"
#include "ctx.hpp"
#include "frame/master_clock.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"
#include "mdns/mdns.hpp"
#include "replies/dict_kv.hpp"

#include <algorithm>
#include <iterator>

namespace pierre {
namespace rtsp {

Setup::Setup(const uint8v &content_in, const Headers &headers_in, Reply &reply, Ctx *ctx) noexcept
    : rdict(content_in), headers_in(headers_in), reply(reply), ctx(ctx), reply_dict() {

  reply(RespCode::BadRequest); // default response is BadRequest

  auto rc = rdict.ready();

  if (rc) {
    const auto dict_has_streams = rdict.fetchNode({STREAMS}, PLIST_ARRAY) ? true : false;

    if (dict_has_streams) { // followup SETUP (contains streams data)
      rc = has_streams();
    } else {
      rc = no_streams(); // initial SETUP (streams not present)
    }

    if (rc) { // all is well, finalize the reply
      reply_dict.format_to(reply.content_out);
      reply.headers_out.add(hdr_type::ContentType, hdr_val::AppleBinPlist);
      reply(RespCode::OK);

    } else {
      INFO(module_id, "WARN", "implementation missing for control_type={}\n", 1);
    }
  }
}

bool Setup::no_streams() noexcept {
  bool rc = true;

  // deduces cat, type and timing
  ctx->setup_stream(rdict.stringView({TIMING_PROTOCOL}));

  // now we create a reply plist based on the type of stream
  if (ctx->stream_info.is_ptp_stream()) {
    ctx->group_id = rdict.stringView({GROUP_UUID});
    ctx->group_contains_group_leader = rdict.boolVal({GROUP_LEADER});

    auto peers = rdict.stringArray({TIMING_PEER_INFO, ADDRESSES});
    ctx->peers(peers);

    Aplist peer_info;

    auto ip_addrs = Host().ip_addresses();
    peer_info.setArray(ADDRESSES, ip_addrs);
    peer_info.setString(ID, ip_addrs[0]);

    reply_dict.setArray(TIMING_PEER_INFO, {peer_info});

    // although unused by AP2 we must set the event port (and listen for connections)
    reply_dict.setUints({{EVENT_PORT, ctx->server_port(EventPort)}, {TIMING_PORT, 0}});

    // adjust Service system flags and request mDNS update
    mDNS::service().receiver_active();
    mDNS::update();

  } else if (ctx->stream_info.is_ntp_stream()) {
    INFO(module_id, "ERROR", "NTP timing not supported\n");
    rc = false;

  } else if (ctx->stream_info.is_remote_control()) {
    // remote control only; listen on event port (same comment as above)
    reply_dict.setUints({{EVENT_PORT, ctx->server_port(EventPort)}, {TIMING_PORT, 0}});
  } else {
    rc = false; // no match
  }

  return rc;
}

// SETUP followup message containing type of stream to start
bool Setup::has_streams() noexcept {
  auto rc = false;

  auto &stream_info = ctx->stream_info;

  Aplist reply_stream0; // reply streams sub dict

  if (stream_info.is_ptp_stream()) {
    // initial setup indicated PTP, the followup setup message finalizes details
    const auto s0 = Aplist(rdict, {STREAMS, IDX0});

    // capture stream info
    stream_info.audio_format = s0.uint({AUDIO_MODE});
    stream_info.ct = (uint8_t)s0.uint({COMPRESSION_TYPE});
    stream_info.conn_id = s0.uint({STREAM_CONN_ID});
    stream_info.spf = s0.uint({SAMPLE_FRAMES_PER_PACKET});

    stream_info.supports_dynamic_stream_id = s0.boolVal({SUPPORTS_DYNAMIC_STREAM_ID});
    stream_info.audio_format = s0.uint({AUDIO_FORMAT});
    stream_info.client_id = s0.stringView({CLIENT_ID});

    ctx->active_remote = headers_in.val<int64_t>(hdr_type::DacpActiveRemote);
    ctx->dacp_id = headers_in.val(hdr_type::DacpID);
    ctx->shared_key = s0.dataArray({SHK}); // copy is ok here

    auto stream_type = ctx->setup_stream_type(s0.uint({TYPE}));

    // now handle the specific stream type
    if (stream_info.is_buffered()) {
      static const toml::path cfg_buff_size{"rtsp.audio.buffer_size.bytes"};
      rc = true;

      const uint64_t buff_size = config()->at(cfg_buff_size).value_or(0x800000);

      // reply requires the type, audio data port and our buffer size
      reply_stream0.setUints({{TYPE, stream_type},                            // stream type
                              {DATA_PORT, ctx->server_port(rtsp::AudioPort)}, // audio port
                              {BUFF_SIZE, buff_size}});                       // our buffer size

      ctx->set_live();

    } else if (stream_info.is_realtime()) {
      rc = false;
      INFO(module_id, "STREAM", "realtime not supported\n");

    } else if (stream_info.is_remote_control()) {
      rc = true;

      reply_stream0.setUints({
          {STREAM_ID, 1},
          {TYPE, stream_type},
      });
    }

  } else if (stream_info.is_ntp_stream()) {
    INFO(module_id, "STREAM", "ntp timing not supported\n");

  } else {
    INFO(module_id, "STREAM", "unknown / unsupported stream\n");
  }

  if (rc) {
    // regardless of stream type start the control server
    reply_stream0.setUint(CONTROL_PORT, ctx->server_port(rtsp::ControlPort));

    // put the sub dict into the reply
    reply_dict.setArray(STREAMS, reply_stream0);
  }

  return rc;
}

} // namespace rtsp
} // namespace pierre