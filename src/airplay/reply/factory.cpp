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

namespace pierre {
namespace airplay {
namespace reply {

shReply Factory::create(const reply::Inject &di) {
  csv method = di.method;
  csv path = di.path;

  __LOGX(LCOL01 " cseq={} method={} path={}\n", moduleId, csv("CREATE"),
         di.headers.getVal(hdr_type::CSeq), method, path);

  if (method.starts_with("CONTINUE")) {
    return std::make_shared<LoadMore>();
  }

  // NOTE: compare sequence equivalent to AirPlay conversation
  if (method == csv("GET") && (path == csv("/info"))) {
    return std::make_shared<Info>();
  }

  if (method == csv("POST")) {
    // NOTE:  all post requests default to OK response code
    if (path == csv("/fp-setup")) {
      return std::make_shared<FairPlay>();
    }

    if (path == csv("/command")) {
      return std::make_shared<Command>();
    }

    if (path == csv("/feedback")) {
      return std::make_shared<Feedback>();
    }

    if (path.starts_with("/pair-")) {
      static const auto pair_paths = std::vector{"/pair-setup", "/pair-verify"};
      if (auto found = ranges::find(pair_paths, path); found != pair_paths.end()) {
        return std::make_shared<Pairing>();
      } else {
        __LOG0(LCOL01 "unhandled pair path: {}\n", moduleId, LBLANK, path);
      }
    }
  }

  if (method == csv("OPTIONS") && (path == csv("*"))) {
    return std::make_shared<Options>();
  }

  if (method == csv("SETUP")) {
    return std::make_shared<Setup>();
  }

  if (method.starts_with("GET_PARAMETER") || method.starts_with("SET_PARAMETER")) {
    return std::make_shared<Parameter>();
  }

  if (method == csv("RECORD")) {
    return std::make_shared<Record>();
  }

  if (method == csv("SETPEERS")) {
    return std::make_shared<SetPeers>();
  }

  if (method == csv("SETPEERSX")) {
    return std::make_shared<SetPeersX>();
  }

  if (method == csv("SETRATEANCHORTIME")) {
    return std::make_shared<SetAnchor>();
  }

  if (method == csv("TEARDOWN")) {
    return std::make_shared<Teardown>();
  }

  if (method == csv("FLUSHBUFFERED")) {
    return std::make_shared<FlushBuffered>();
  }

  __LOG0(LCOL01 "method={} path={}]\n", moduleId, csv("FAILED"), //
         method.empty() ? "<empty>" : method, path.empty() ? "<empty>" : path);

  return std::make_shared<Unhandled>();
}

} // namespace reply
} // namespace airplay
} // namespace pierre
