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
//  GNU General Public License for more detsils.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#include <cstdint>
#include <fmt/format.h>
#include <memory>
#include <source_location>

#include "rtp/stream_info.hpp"

namespace pierre {
namespace rtp {

// this is overkill for this simple of a class
// goal is to have at least well-formed user object copy/assignment example

// construct with StreamData (copy)
StreamInfo::StreamInfo(const StreamData &sd) : data(sd) { __init(); }

// construct with StreamData (move)
StreamInfo::StreamInfo(const StreamData &&sd) : data(std::move(sd)) { __init(); }

// construct from StreamInfo (copy)
StreamInfo::StreamInfo(const StreamInfo &si) : data(si.data) { __init(); }

// construct from StreamInfo (move)
StreamInfo::StreamInfo(StreamInfo &&si) : data(std::move(si.data)) { __init(); }

// assignment (copy)
StreamInfo &StreamInfo::operator=(const StreamData &sd) {
  data = sd;

  __init();

  return *this;
}

// assignment (move)
StreamInfo &StreamInfo::operator=(StreamData &&sd) {
  data = std::move(sd);

  __init();

  return *this;
}

// assignment (copy)
StreamInfo &StreamInfo::operator=(const StreamInfo &si) {
  data = si.data;

  __init();

  return *this;
}

// assignment (move)
StreamInfo &StreamInfo::operator=(StreamInfo &&si) {
  data = std::move(si.data);

  __init();

  return *this;
}

void StreamInfo::__init() {
  // nothing to init at the moment, might be later
}

void StreamInfo::teardown() {
  // this is a "partial" teardown, just clear some items
  data.key.clear();
  data.active_remote.clear();
  data.dacp_id.clear();
}

void StreamInfo::dump(const std::source_location loc) {
  const auto prefix = loc.function_name();
  const auto fmt_str = FMT_STRING("{:>15}={}\n");

  fmt::print("{}\n", prefix);
  fmt::print(fmt_str, "shk", (data.key.size() > 0));
  fmt::print(fmt_str, "Active-Remote", data.active_remote);
  fmt::print(fmt_str, "DACP-ID", data.dacp_id);
}

} // namespace rtp
} // namespace pierre