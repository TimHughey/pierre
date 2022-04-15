/*
    Pierre - Custom Light Show for Wiss Landing
    Copyright (C) 2022  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#pragma once

#include "rtsp/aplist.hpp"
#include "rtsp/reply.hpp"

namespace pierre {
namespace rtsp {

class Setup : public Reply, Aplist {
public:
  enum StreamCategory : uint8_t { UnknownStream = 0, PtpStream };
  enum TimingType : uint8_t { UnknownTiming = 0, TsPtp };

public:
  Setup(sRequest request);

  bool populate() override;

private:
  bool getGroupInfo();
  bool getTimingList();
  bool checkTimingProtocol();

private:
  std::vector<plist_t> _plists;
  plist_t *_pl_in = nullptr;
  plist_t *_pl_out = nullptr;

  Aplist _reply_pl;

  TimingType _timing_type = UnknownTiming;
  StreamCategory _stream_category = UnknownStream;
  string _group_uuid;
  bool _group_contains_leader = false;
  Aplist::ArrayStrings _timing_peer_info;
};

} // namespace rtsp
} // namespace pierre