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

#include "conn_info/stream_info.hpp"

#include <cstdint>
#include <fmt/format.h>
#include <memory>

namespace pierre {
namespace airplay {

// this is overkill for this simple of a class
// goal is to have at least well-formed user object copy/assignment example

// construct with StreamData (copy)
StreamInfo::StreamInfo(const StreamData &sd) noexcept : _data(sd) {}

// construct with StreamData (move)
StreamInfo::StreamInfo(const StreamData &&sd) noexcept : _data(std::move(sd)) {}

// construct from StreamInfo (copy)
StreamInfo::StreamInfo(const StreamInfo &si) noexcept : _data(si._data) {}

// construct from StreamInfo (move)
StreamInfo::StreamInfo(StreamInfo &&si) noexcept : _data(std::move(si._data)) {}

// assignment (copy)
StreamInfo &StreamInfo::operator=(const StreamData &sd) noexcept {
  _data = sd;

  return *this;
}

// assignment (move)
StreamInfo &StreamInfo::operator=(StreamData &&sd) noexcept {
  _data = std::move(sd);

  return *this;
}

// assignment (copy)
StreamInfo &StreamInfo::operator=(const StreamInfo &si) noexcept {
  _data = si._data;

  return *this;
}

// assignment (move)
StreamInfo &StreamInfo::operator=(StreamInfo &&si) noexcept {
  _data = std::move(si._data);

  return *this;
}

void StreamInfo::teardown() {
  // this is a "partial" teardown, just clear some items
  _data.key.clear();
  _data.active_remote.clear();
  _data.dacp_id.clear();
}

void StreamInfo::dump() {
  const auto fmt_str = FMT_STRING("{:>15}={}\n");

  fmt::print(fmt_str, "shk", (_data.key.size() > 0));
  fmt::print(fmt_str, "Active-Remote", _data.active_remote);
  fmt::print(fmt_str, "DACP-ID", _data.dacp_id);
}

} // namespace airplay
} // namespace pierre
