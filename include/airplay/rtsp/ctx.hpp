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
#include "base/uint8v.hpp"

#include <memory>

namespace pierre {
namespace rtsp {

struct stream {
  enum class cat : uint64_t { unspecified = 10, ptp_stream, ntp_stream, remote_control };
  enum class type : uint64_t { none = 0, realtime = 96, buffered = 103 };
  enum class timing : uint64_t { none = 0, ntp, ptp };

  constexpr uint64_t typeBuffered() { return 103; }
  constexpr uint64_t typeRealTime() { return 96; }

  struct {
    bool supports_dynamic_stream_id{false};
    string client_id;
    uint64_t audio_format{0};
    uint64_t audio_mode{0};
    uint64_t conn_id{0}; // stream connection id
    uint64_t ct{0};      // compression type
    uint64_t spf{0};     // sample frames per packet
    uint64_t type{0};    // stream::type (buffered or realtime)
    uint8v key;          // shared key (for decipher)
  } info;
};

class Ctx : public std::enable_shared_from_this<Ctx> {

public:
  static auto create() { return std::shared_ptr<Ctx>(new Ctx()); }
  auto ptr() noexcept { return shared_from_this(); }

private:
  Ctx() noexcept : cseq{0}, active_remote{0} {}

public:
  // from RTSP headers
  int64_t cseq;          // CSeq: 8 (message seq num for active session, increasing from zero)
  int64_t active_remote; // Active-Remote: 1570223890
  string dacp_id;        // DACP-ID: DF86B6D21A6C805F
  string user_agent;     // User-Agent: AirPlay/665.13.1
  int64_t proto_ver;     // X-Apple-ProtocolVersion: 1
  string client_name;    // X-Apple-Client-Name: xapham

  string group_id; // airplay group id
  bool group_contains_group_leader{false};

  stream::cat stream_cat{stream::cat::unspecified};
  stream::timing stream_timing{stream::timing::none};
  stream::type stream_type{stream::type::none};

public:
  static constexpr csv module_id{"RTSP_CTX"};
};

} // namespace rtsp
} // namespace pierre