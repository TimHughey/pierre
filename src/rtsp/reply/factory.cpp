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

#include <algorithm>
#include <array>
#include <fmt/format.h>
#include <source_location>
#include <string_view>
#include <unordered_set>

#include "rtsp/reply/all.hpp"
namespace pierre {
namespace rtsp {

namespace reply {

using std::find_if, std::array;
using strv = std::string_view;

static const char *fnName(std::source_location loc = std::source_location::current()) {
  return loc.function_name();
}

sReply Factory::create(const Reply::Opts &opts) {
  const std::string_view method = opts.method;
  const std::string_view path = opts.path;

  if (method.starts_with("CONTINUE")) {
    return std::make_shared<LoadMore>(opts);
  }

  // NOTE: compare sequence equivalent to AirPlay conversation
  if (method.compare("GET") == 0) {
    if (path.compare("/info") == 0) {
      return std::make_shared<Info>(opts);
    }
  }

  if (method.compare("POST") == 0) {
    // NOTE:  all post requests default to OK response code
    if (path.compare("/fp-setup") == 0) {
      return std::make_shared<FairPlay>(opts);
    }

    if (path.compare("/command") == 0) {
      return std::make_shared<Command>(opts);
    }

    if (path.compare("/feedback") == 0) {
      return std::make_shared<Feedback>(opts);
    }
    if (path.starts_with("/pair-")) {
      auto pair_paths = array{"/pair-setup", "/pair-verify"};
      auto check = [path](const auto p) { return path.compare(p) == 0; };
      auto found = find_if(pair_paths.begin(), pair_paths.end(), check);

      if (found != pair_paths.end()) {
        return std::make_shared<Pairing>(opts);
      } else {
        fmt::print("Factory::create(): unhandled pair path: {}\n", path);
      }
    }
  }

  if (method.compare("OPTIONS") == 0) {
    if (path.compare("*") == 0) {
      return std::make_shared<Options>(opts);
    }
  }

  if (method.compare("SETUP") == 0) {
    return std::make_shared<Setup>(opts);
  }

  if (method.starts_with("GET_PARAMETER") || method.starts_with("SET_PARAMETER")) {
    return std::make_shared<Parameter>(opts);
  }

  if (method.compare("RECORD") == 0) {
    return std::make_shared<Record>(opts);
  }

  if (method.starts_with("SETPEERS")) {
    return std::make_shared<SetPeers>(opts);
  }

  if (method.starts_with("SETRATEANCHORTIME")) {
    return std::make_shared<Anchor>(opts);
  }

  if (method.starts_with("TEARDOWN")) {
    return std::make_shared<Teardown>(opts);
  }

  if (method.starts_with("FLUSHBUFFERED")) {
    return std::make_shared<FlushBuffered>(opts);
  }

  const auto loc = std::source_location::current();
  fmt::print("In file {} at line {}\n", loc.file_name(), loc.line());
  const auto log_method = (method.empty()) ? "<empty>" : method;
  const auto log_path = (path.empty()) ? "<empty>" : path;
  fmt::print("\t{}: unhandled method={} path={}\n\n", fnName(), log_method, log_path);

  return std::make_shared<Unhandled>(opts);
}

} // namespace reply
} // namespace rtsp
} // namespace pierre
