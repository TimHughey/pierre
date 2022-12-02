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

#include "base/logger.hpp"
#include "base/types.hpp"
#include "reply/all.hpp"

#include <set>

namespace pierre {
namespace airplay {
namespace reply {

[[nodiscard]] shReply Factory::create(const reply::Inject &di) {
  static const std::set<string_view> pair_paths{"/pair-setup", "/pair-verify"};

  shReply reply;

  csv method = di.method;
  csv path = di.path;

  INFOX(moduleId, "CREATE", "cseq={} method={} path={}\n", di.headers.val<int64_t>(hdr_type::CSeq),
        method, path);

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
    } else if (pair_paths.contains(path)) {
      reply = std::make_shared<Pairing>();
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

    INFO(moduleId, "FAILED", "method={} path={}]\n", //
         method.empty() ? "<empty>" : method, path.empty() ? "<empty>" : path);

    reply = std::make_shared<Unhandled>();
  }

  reply->inject(di);

  return reply;
}

} // namespace reply
} // namespace airplay
} // namespace pierre
