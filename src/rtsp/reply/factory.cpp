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
#include <source_location>
#include <string_view>
#include <unordered_set>

#include "fmt/format.h"
#include "rtsp/reply/command.hpp"
#include "rtsp/reply/factory.hpp"
#include "rtsp/reply/fairplay.hpp"
#include "rtsp/reply/feedback.hpp"
#include "rtsp/reply/info.hpp"
#include "rtsp/reply/options.hpp"
#include "rtsp/reply/pairing.hpp"
#include "rtsp/reply/parameter.hpp"
#include "rtsp/reply/record.hpp"
#include "rtsp/reply/set_peers.hpp"
#include "rtsp/reply/setup.hpp"
#include "rtsp/reply/teardown.hpp"
#include <rtsp/reply/anchor.hpp>
#include <rtsp/reply/load_more.hpp>
#include <rtsp/reply/unhandled.hpp>

namespace pierre {
namespace rtsp {

namespace reply {

using std::find_if, std::array;
using strv = std::string_view;
// typedef const std::unordered_set<strv> NoLogSet;

// static NoLogSet __nolog = {strv("/feedback"),   strv("/info"),     strv("/pair-verify"),
//                            strv("/pair-setup"), strv("/fp-setup"), strv("/command")};

static const char *fnName(std::source_location loc = std::source_location::current()) {
  return loc.function_name();
}

sReply Factory::create(const Reply::Opts &opts) {
  const std::string_view method = opts.method;
  const std::string_view path = opts.path;

  if (method.starts_with("CONTINUE")) {
    // fmt::print("{} creating Continue\n", fnName());
    return std::make_shared<LoadMore>(opts);
  }

  // if (__nolog.contains(path) == false) {
  //   fmt::print("{} mathod={} path={}\n", fnName(), method, path);
  // }

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
