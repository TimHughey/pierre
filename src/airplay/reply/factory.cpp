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

#include "base/typical.hpp"
#include "reply/all.hpp"

#include <algorithm>
#include <array>
#include <ranges>

namespace pierre {
namespace airplay {
namespace reply {

[[nodiscard]] shReply Factory::create(const reply::Inject &di) {
  static const auto pair_paths = std::vector{"/pair-setup", "/pair-verify"};

  shReply reply;

  csv method = di.method;
  csv path = di.path;

  __LOGX(LCOL01 " cseq={} method={} path={}\n", moduleId, csv("CREATE"),
         di.headers.getVal(hdr_type::CSeq), method, path);

  if (method.starts_with("CONTINUE")) {
    reply = std::make_shared<LoadMore>();
  } else if (method == csv("GET") && (path == csv("/info"))) {
    reply = std::make_shared<Info>();
  } else if (method == csv("POST")) {
    // NOTE:  all post requests default to OK response code
    if (path == csv("/fp-setup")) {
      reply = std::make_shared<FairPlay>();
    } else if (path == csv("/command")) {
      reply = std::make_shared<Command>();
    } else if (path == csv("/feedback")) {
      reply = std::make_shared<Feedback>();
    } else if (path.starts_with("/pair-")) {
      if (ranges::any_of(pair_paths, [&path = path](csv p) { return p == path; })) {
        reply = std::make_shared<Pairing>();
      } else {
        __LOG0(LCOL01 "unhandled pairing path={}\n", moduleId, //
               csv("CREATE"), path);
      }
    }
  } else if (method == csv("OPTIONS") && (path == csv("*"))) {
    reply = std::make_shared<Options>();
  } else if (method == csv("SETUP")) {
    reply = std::make_shared<Setup>();
  } else if ((method == csv("GET_PARAMETER")) || (method == csv("SET_PARAMETER"))) {
    reply = std::make_shared<Parameter>();
  } else if (method == csv("RECORD")) {
    reply = std::make_shared<Record>();
  } else if (method == csv("SETPEERS")) {
    reply = std::make_shared<SetPeers>();
  } else if (method == csv("SETPEERSX")) {
    reply = std::make_shared<SetPeersX>();
  } else if (method == csv("SETRATEANCHORTIME")) {
    reply = std::make_shared<SetAnchor>();
  } else if (method == csv("TEARDOWN")) {
    reply = std::make_shared<Teardown>();
  } else if (method == csv("FLUSHBUFFERED")) {
    reply = std::make_shared<FlushBuffered>();
  } else if (reply.use_count() == 0) {

    __LOG0(LCOL01 "method={} path={}]\n", moduleId, csv("FAILED"), //
           method.empty() ? "<empty>" : method, path.empty() ? "<empty>" : path);

    reply = std::make_shared<Unhandled>();
  }

  reply->inject(di);

  return reply;
}

} // namespace reply
} // namespace airplay
} // namespace pierre
