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
  const string_view method = di.method;
  const string_view path = di.path;

  __LOGX(LCOL0 LCOL1 "cseq={} method={} path={}\n", moduleId, csv("CREATE"),
         di.headers.getVal(header::type::CSeq), method, path);

  if (method.starts_with("CONTINUE")) {
    return std::make_shared<LoadMore>();
  }

  // NOTE: compare sequence equivalent to AirPlay conversation
  if (method.compare("GET") == 0) {
    if (path.compare("/info") == 0) {
      return std::make_shared<Info>();
    }
  }

  if (method.compare("POST") == 0) {
    // NOTE:  all post requests default to OK response code
    if (path.compare("/fp-setup") == 0) {
      return std::make_shared<FairPlay>();
    }

    if (path.compare("/command") == 0) {
      return std::make_shared<Command>();
    }

    if (path.compare("/feedback") == 0) {
      return std::make_shared<Feedback>();
    }
    if (path.starts_with("/pair-")) {
      auto pair_paths = std::array{"/pair-setup", "/pair-verify"};
      auto check = [path](const auto p) { return path.compare(p) == 0; };
      auto found = std::find_if(pair_paths.begin(), pair_paths.end(), check);

      if (found != pair_paths.end()) {
        return std::make_shared<Pairing>();
      } else {
        __LOG0(LCOL01 "unhandled pair path: {}\n", moduleId, LBLANK, path);
      }
    }
  }

  if (method.compare("OPTIONS") == 0) {
    if (path.compare("*") == 0) {
      return std::make_shared<Options>();
    }
  }

  if (method.compare("SETUP") == 0) {
    return std::make_shared<Setup>();
  }

  if (method.starts_with("GET_PARAMETER") || method.starts_with("SET_PARAMETER")) {
    return std::make_shared<Parameter>();
  }

  if (method.compare("RECORD") == 0) {
    return std::make_shared<Record>();
  }

  if (method == csv("SETPEERS")) {
    return std::make_shared<SetPeers>();
  }

  if (method == csv("SETPEERSX")) {
    return std::make_shared<SetPeersX>();
  }

  if (method.starts_with("SETRATEANCHORTIME")) {
    return std::make_shared<SetAnchor>();
  }

  if (method.starts_with("TEARDOWN")) {
    return std::make_shared<Teardown>();
  }

  if (method.starts_with("FLUSHBUFFERED")) {
    return std::make_shared<FlushBuffered>();
  }

  __LOG0(LCOL01 "method={} path={}]\n", moduleId, csv("FAILED"),
         method.empty() ? "<empty>" : method, path.empty() ? "<empty>" : path);

  return std::make_shared<Unhandled>();
}

} // namespace reply
} // namespace airplay
} // namespace pierre
