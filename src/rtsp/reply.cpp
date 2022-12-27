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
#include "base/logger.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "frame/master_clock.hpp"
#include "frame/racked.hpp"
#include "headers.hpp"
#include "mdns/mdns.hpp"
#include "rtsp/aes_ctx.hpp"
#include "rtsp/aplist.hpp"
#include "rtsp/ctx.hpp"
#include "rtsp/replies/command.hpp"
#include "rtsp/replies/dict_kv.hpp"
#include "rtsp/replies/fairplay.hpp"
#include "rtsp/replies/info.hpp"
#include "rtsp/replies/set_anchor.hpp"
#include "rtsp/replies/setup.hpp"

#include <algorithm>
#include <iterator>

namespace pierre {

namespace rtsp {

void Reply::build(Request &request, std::shared_ptr<Ctx> ctx) noexcept {

  // get a naked pointer to ctx for use locally to save shared_ptr dereferences
  // this is OK because this function receives the shared_ptr and therefore keeps
  // it in scope until it returns.
  auto *ctx_naked = ctx.get();

  // handle the various RTSP requests based on the method and path
  const auto &method = request.headers.method();
  const auto &path = request.headers.path();

  // all replies must include CSeq and Server headers, copy/add them now
  headers.copy(hdr_type::CSeq, request.headers);
  headers.add(hdr_type::Server, hdr_val::AirPierre);

  if (method.starts_with("CONTINUE")) {
    resp_code(RespCode::Continue); // trivial, only set reqponse code

  } else if (method == csv("GET") && (path == csv("/info"))) {
    Info(request, *this);

  } else if (method == csv("POST")) { // handle POST methods

    if (path == csv("/fp-setup")) {
      FairPlay(request, *this);

    } else if (path == csv("/command")) {
      Command(request, *this);

    } else if (path == csv("/feedback")) {
      // trival, basic headers and response code of OK
      resp_code(RespCode::OK);
      ctx_naked->feedback_msg();

    } else if (path.starts_with("/pair-")) {
      // pairing setup and verify
      AesResult aes_result;

      if (path.ends_with("setup")) {
        aes_result = ctx_naked->aes_ctx.setup(request.content, content);
      } else if (path.ends_with("verify")) {
        aes_result = ctx_naked->aes_ctx.verify(request.content, content);
      }

      if (has_content()) {
        headers.add(hdr_type::ContentType, hdr_val::OctetStream);
      }

      resp_code = aes_result.resp_code;
    }
  } else if (method == csv("OPTIONS") && (path == csv("*"))) {
    // trivial, only populate an additional header with a string
    // containing the available message types (aka options)
    static const string options{
        "ANNOUNCE, SETUP, RECORD, PAUSE, FLUSH, FLUSHBUFFERED, "
        "TEARDOWN, OPTIONS, POST, GET, PUT"};

    headers.add(hdr_type::Public, options);
    set_resp_code(RespCode::OK);

  } else if (method == csv("SETUP")) {
    Setup(request, *this, ctx_naked);

  } else if (method.ends_with("_PARAMETER")) {
    if (method.starts_with("GET")) {
      csv param = request.content.view();

      if (param.starts_with("volume")) {
        constexpr csv full_volume("\r\nvolume: 0.0\r\n");

        copy_to_content(full_volume);
        headers.add(hdr_type::ContentType, hdr_val::TextParameters);
      }

    } else if (method.starts_with("SET")) {
      // not using volume at this time
    }

    set_resp_code(RespCode::OK);

  } else if (method == csv("RECORD")) {
    // trivial, respond OK
    set_resp_code(RespCode::OK);

  } else if (method == csv("SETPEERS")) {

    Aplist request_dict(request.content);
    auto peers = request_dict.stringArray({ROOT});
    if (peers.empty() == false) {
      MasterClock::peers(peers);   // set the peer lists
      set_resp_code(RespCode::OK); // indicate success
    }

  } else if (method == csv("SETPEERSX")) {

    Aplist request_dict = request.content;

    MasterClock::Peers peer_list;
    const auto count = request_dict.arrayItemCount({ROOT});

    for (uint32_t idx = 0; idx < count; idx++) {
      auto idxs = fmt::format("{}", idx);
      if (auto peers = request_dict.stringArray({csv(idxs), ADDRESSES}); peers.size() > 0) {
        ranges::copy(peers.begin(), peers.end(), std::back_inserter(peer_list));

        set_resp_code(RespCode::OK); // we got some peer addresses
      }
    }

    MasterClock::peers(peer_list);

  } else if (method == csv("SETRATEANCHORTIME")) {
    SetAnchor(request, *this);
  } else if (method == csv("TEARDOWN")) {

    Aplist request_dict(request.content);

    headers.add(hdr_type::ContentSimple, hdr_val::ConnectionClosed);
    set_resp_code(RespCode::OK); // always OK

    // any TEARDOWN request (with streams key or not) always clears the shared key and
    // informs Racked spooling should be stopped
    ctx_naked->shared_key.clear();
    Racked::spool(false);

    // when the streams key is not present this is a complete disconnect
    if (request_dict.exists(STREAMS) == false) {
      mDNS::service().receiver_active(false);
      mDNS::update();

      Racked::flush_all();
      ctx_naked->teardown();
    }

  } else if (method == csv("FLUSHBUFFERED")) {

    Aplist request_dict(request.content);

    // notes:
    // 1. from_seq and from_ts may not be present
    // 2. until_seq and until_ts should always be present
    Racked::flush(                                      //
        FlushInfo(request_dict.uint({FLUSH_FROM_SEQ}),  //
                  request_dict.uint({FLUSH_FROM_TS}),   //
                  request_dict.uint({FLUSH_UNTIL_SEQ}), //
                  request_dict.uint({FLUSH_UNTIL_TS}))  //
    );

    set_resp_code(RespCode::OK);

  } else if (resp_code == RespCode::NotImplemented) {

    INFO(module_id, "FAILED", "method={} path={}]\n", //
         method.empty() ? "<empty>" : method, path.empty() ? "<empty>" : path);
  }

  wire.clear();

  auto w = std::back_inserter(wire);
  constexpr csv seperator{"\r\n"};

  fmt::format_to(w, "RTSP/1.0 {}{}", resp_code(), seperator);

  // must add content length before calling headers list()
  if (content.empty() == false) {
    headers.add(hdr_type::ContentLength, content.size());
  }

  headers.format_to(w);

  // always write the separator between headers and content
  fmt::format_to(w, "{}", seperator);

  if (content.empty() == false) { // we have content, add it
    std::copy(content.begin(), content.end(), w);
  }
}

} // namespace rtsp
} // namespace pierre
