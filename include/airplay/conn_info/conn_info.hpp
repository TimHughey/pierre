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

#pragma once

#include "base/types.hpp"
#include "conn_info/stream.hpp"
#include "conn_info/stream_info.hpp"

#include <memory>
#include <optional>

namespace pierre {
namespace airplay {

class ConnInfo;
typedef std::shared_ptr<ConnInfo> shConnInfo;

namespace shared {
std::optional<shConnInfo> &connInfo();
} // namespace shared

class ConnInfo : public std::enable_shared_from_this<ConnInfo> {

public:
  static shConnInfo init() { return shared::connInfo().emplace(new ConnInfo()); }
  static shConnInfo ptr() { return shared::connInfo().value()->shared_from_this(); }
  static void reset() { shared::connInfo().reset(); }

private:
  ConnInfo() = default; // all access through shared ptr

public:
  static constexpr size_t bufferSize() { return 1024 * 1024 * 8; };

  // setters
  void save(const Stream &new_stream) { stream = new_stream; }
  void save(const StreamData &data);

  StreamInfo stream_info;
  Stream stream;

  // captured from RTSP SETUP initial message (no stream data)
  string airplay_gid; // UUID in the Bonjour advertisement -- if NULL, the group
                      // UUID is the same as the pi UUID

  // captured from RTSP SETUP (no stream data)
  bool groupContainsGroupLeader = false;
  // csv ops_mode = ops_mode::INITIALIZE;

  // string dacp_active_remote; // key to send to the remote controller

private:
  string dacp_id; // id of the client -- used to find the port to be used
};
} // namespace airplay

} // namespace pierre