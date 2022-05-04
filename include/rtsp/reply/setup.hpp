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

#include <unordered_map>
#include <vector>

#include "packet/aplist.hpp"
#include "rtsp/reply.hpp"

namespace pierre {
namespace rtsp {

class Setup : public Reply, packet::Aplist {
public:
  using string = std::string;

public:
  enum StreamType : uint8_t { Unknown = 0, Buffered, RealTime, NTP };
  enum TimingProtocol : uint8_t { None = 0, PreciseTiming, NetworkTime };
  enum DictKey : uint8_t {
    audioMode = 0,
    ct,
    streamConnectionID,
    spf,
    shk,
    supportsDyanamicStreamID,
    audioFormat,
    clientID,
    type,
    controlPort,
    dataPort,
    audioBufferSize,
    streams,
    timingProtocol
  };

  typedef std::vector<bool> Checks;
  typedef std::unordered_map<DictKey, ccs> DictKeyMap;
  typedef std::unordered_map<StreamType, uint8_t> StreamTypeMap;
  typedef std::unordered_map<TimingProtocol, ccs> TimingProtocolMap;

public:
  Setup(const Reply::Opts &opts);

  bool populate() override;

private:
  bool checksOK() const;
  void checksReset() { _checks.clear(); }

  ccs dictKey(DictKey key) const { return _key_map[key]; }

  void getGroupInfo();
  void getTimingList();

  bool handleNoStreams();
  bool handleStreams();

  ccs timingProtocolVal(TimingProtocol protocol) const { return _timing_protocol_map[protocol]; }

  void validateTimingProtocol();

  // capture the bool result of a get or set
  inline bool saveCheck(bool rc) { return _checks.emplace_back(rc); }

private:
  // results of get/set operations
  Checks _checks;

  string _group_uuid;
  bool _group_contains_leader = false;
  Aplist::ArrayStrings _timing_peer_info;
  string _session_key;

  static DictKeyMap _key_map;
  static StreamTypeMap _stream_type_map;
  static TimingProtocolMap _timing_protocol_map;
};

} // namespace rtsp
} // namespace pierre