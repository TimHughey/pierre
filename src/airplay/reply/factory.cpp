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

#include "common/typedefs.hpp"
#include "reply/all.hpp"

#include <algorithm>
#include <array>
#include <fmt/format.h>
#include <source_location>
#include <string_view>
#include <unordered_set>

namespace pierre {
namespace airplay {
namespace reply {

namespace header = pierre::packet::header;

shReply Factory::create(const reply::Inject &di) {
  const std::string_view method = di.method;
  const std::string_view path = di.path;

  if (false) { // debug
    constexpr auto f = FMT_STRING("{} FACTORY cseq={} method={} path={}\n");
    fmt::print(f, runTicks(), di.headers.getVal(header::type::CSeq), method, path);
  }

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
        fmt::print("Factory::create(): unhandled pair path: {}\n", path);
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

  if (method.starts_with("SETPEERS")) {
    return std::make_shared<SetPeers>();
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

  if (true) {
    const auto log_method = (method.empty()) ? "<empty>" : method;
    const auto log_path = (path.empty()) ? "<empty>" : path;
    constexpr auto f = FMT_STRING("{} FACTORY FAILED method={} path={}\n");
    fmt::print(f, runTicks(), log_method, log_path);
  }

  return std::make_shared<Unhandled>();
}

} // namespace reply
} // namespace airplay
} // namespace pierre
