//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2022  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#include "rtsp/reply.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "desk/desk.hpp"
#include "frame/master_clock.hpp"
#include "headers.hpp"
#include "lcs/logger.hpp"
#include "mdns/mdns.hpp"
#include "rtsp/aes.hpp"
#include "rtsp/aplist.hpp"
#include "rtsp/ctx.hpp"
#include "rtsp/replies/command.hpp"
#include "rtsp/replies/dict_kv.hpp"
#include "rtsp/replies/fairplay.hpp"
#include "rtsp/replies/info.hpp"
#include "rtsp/replies/set_anchor.hpp"
#include "rtsp/replies/setup.hpp"
#include "rtsp/rtsp.hpp"

#include <algorithm>
#include <fmt/format.h>
#include <iterator>

namespace pierre {

namespace rtsp {

void Reply::build(Ctx *ctx, const Headers &headers_in, const uint8v &content_in) noexcept {
  static constexpr csv fn_id{"build"};

  // handle the various RTSP requests based on the method and path
  const auto &method = headers_in.method();
  const auto &path = headers_in.path();

  INFO(module_id, fn_id, "method={} path={}\n", method, path);

  // all replies must include CSeq and Server headers, copy/add them now
  headers_out.copy(hdr_type::CSeq, headers_in);
  headers_out.add(hdr_type::Server, hdr_val::AirPierre);

  if (method.starts_with("CONTINUE")) {
    resp_code(RespCode::Continue); // trivial, only set reqponse code

  } else if (method == csv("GET") && (path == csv("/info"))) {
    Info(*this);

  } else if (method == csv("POST")) { // handle POST methods

    if (path == csv("/fp-setup")) {
      FairPlay(content_in, *this);

    } else if (path == csv("/command")) {
      Command(content_in, *this);

    } else if (path == csv("/feedback")) {
      // trival, basic headers and response code of OK
      resp_code(RespCode::OK);
      ctx->feedback_msg();

    } else if (path.starts_with("/pair-")) {
      // pairing setup and verify
      AesResult aes_result;

      if (path.ends_with("setup")) {
        aes_result = ctx->aes.setup(content_in, content_out);
      } else if (path.ends_with("verify")) {
        aes_result = ctx->aes.verify(content_in, content_out);
      }

      if (has_content()) {
        headers_out.add(hdr_type::ContentType, hdr_val::OctetStream);
      }

      resp_code = aes_result.resp_code;
    }
  } else if (method == csv("OPTIONS") && (path == csv("*"))) {
    // trivial, only populate an additional header with a string
    // containing the available message types (aka options)
    static const string options{
        "ANNOUNCE, SETUP, RECORD, PAUSE, FLUSH, FLUSHBUFFERED, "
        "TEARDOWN, OPTIONS, POST, GET, PUT"};

    headers_out.add(hdr_type::Public, options);
    set_resp_code(RespCode::OK);

  } else if (method == csv("SETUP")) {
    Setup(content_in, headers_in, *this, ctx);

  } else if (method.ends_with("_PARAMETER")) {
    if (method.starts_with("GET")) {
      csv param = content_in.view();

      if (param.starts_with("volume")) {
        constexpr csv full_volume("\r\nvolume: 0.0\r\n");

        copy_to_content(full_volume);
        headers_out.add(hdr_type::ContentType, hdr_val::TextParameters);
      }

    } else if (method.starts_with("SET")) {
      // not using volume at this time
    }

    set_resp_code(RespCode::OK);

  } else if (method == csv("RECORD")) {

    // trivial, respond OK
    set_resp_code(RespCode::OK);

  } else if (method == csv("SETPEERS")) {

    Aplist request_dict(content_in);
    auto peers = request_dict.stringArray({ROOT});
    if (peers.empty() == false) {
      ctx->peers(peers);           // set the peer lists
      set_resp_code(RespCode::OK); // indicate success
    }

  } else if (method == csv("SETPEERSX")) {

    Aplist request_dict = content_in;

    Peers peer_list;
    const auto count = request_dict.arrayItemCount({ROOT});

    for (uint32_t idx = 0; idx < count; idx++) {
      auto idxs = fmt::format("{}", idx);
      if (auto peers = request_dict.stringArray({csv(idxs), ADDRESSES}); peers.size() > 0) {
        ranges::copy(peers.begin(), peers.end(), std::back_inserter(peer_list));

        set_resp_code(RespCode::OK); // we got some peer addresses
      }
    }

    ctx->master_clock->peers(peer_list);

  } else if (method == csv("SETRATEANCHORTIME")) {
    SetAnchor(content_in, *this, ctx->desk);
  } else if (method == csv("TEARDOWN")) {

    Aplist request_dict(content_in);

    headers_out.add(hdr_type::ContentSimple, hdr_val::ConnectionClosed);
    set_resp_code(RespCode::OK); // always OK

    // any TEARDOWN request (with streams key or not) always clears the shared key and
    // informs Racked spooling should be stopped
    ctx->shared_key.clear();
    ctx->desk->spool(false);

    // when the streams key is not present this is a complete disconnect
    if (request_dict.exists(STREAMS) == false) {
      mDNS::service().receiver_active(false);
      mDNS::update();

      ctx->desk->flush_all();
      ctx->teardown_now = true;
    }

  } else if (method == csv("FLUSHBUFFERED")) {
    Aplist request_dict(content_in);

    // notes:
    // 1. from_seq and from_ts may not be present
    // 2. until_seq and until_ts should always be present
    ctx->desk->flush(                                   //
        FlushInfo(request_dict.uint({FLUSH_FROM_SEQ}),  //
                  request_dict.uint({FLUSH_FROM_TS}),   //
                  request_dict.uint({FLUSH_UNTIL_SEQ}), //
                  request_dict.uint({FLUSH_UNTIL_TS}))  //
    );

    set_resp_code(RespCode::OK);

  } else if (resp_code == RespCode::NotImplemented) {
    error = fmt::format("method={} path={} not implemented", method.empty() ? "<empty>" : method,
                        path.empty() ? "<empty>" : path);

    INFO(module_id, fn_id, "{}\n", error);
  }

  auto w = std::back_inserter(wire);
  constexpr csv seperator{"\r\n"};

  fmt::format_to(w, "RTSP/1.0 {}{}", resp_code(), seperator);

  // must add content length before calling headers list()
  if (content_out.empty() == false) {
    headers_out.add(hdr_type::ContentLength, content_out.size());
  }

  headers_out.format_to(w);

  // always write the separator between headers and content
  fmt::format_to(w, "{}", seperator);

  if (content_out.empty() == false) { // we have content, add it
    std::copy(content_out.begin(), content_out.end(), w);
  }
}

} // namespace rtsp
} // namespace pierre
