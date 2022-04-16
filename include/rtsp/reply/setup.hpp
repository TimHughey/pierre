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

#include <vector>

#include "rtsp/aplist.hpp"
#include "rtsp/reply.hpp"

namespace pierre {
namespace rtsp {

class Setup : public Reply, Aplist {
public:
  using string = std::string;

public:
  enum StreamCategory : uint8_t { UnknownStream = 0, PtpStream };
  enum TimingType : uint8_t { UnknownTiming = 0, TsPtp };

  typedef std::vector<bool> Checks;

public:
  Setup(const Reply::Opts &opts);

  bool populate() override;

private:
  bool checksOK() const;
  void checksReset() { _checks.clear(); }

  void getGroupInfo();
  void getTimingList();
  void validateTimingProtocol();

  // capture the bool result of a get or set
  inline bool saveCheck(bool rc) { return _checks.emplace_back(rc); }

private:
  // results of get/set operations
  Checks _checks;

  TimingType _timing_type = UnknownTiming;
  StreamCategory _stream_category = UnknownStream;

  string _group_uuid;
  bool _group_contains_leader = false;
  Aplist::ArrayStrings _timing_peer_info;
};

} // namespace rtsp
} // namespace pierre