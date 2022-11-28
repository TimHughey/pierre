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

struct stream_info_t {
  enum class cat : uint64_t { unspecified = 10, ptp_stream, ntp_stream, remote_control };
  enum class type : uint64_t { none = 0, realtime = 96, buffered = 103 };
  enum class proto : uint64_t { none = 0, ntp, ptp };

  constexpr bool is_ntp_stream() noexcept { return timing_cat == cat::ntp_stream; }
  constexpr bool is_ptp_stream() noexcept { return timing_cat == cat::ptp_stream; }
  constexpr bool is_remote_control() noexcept { return timing_cat == cat::remote_control; }
  constexpr bool is_buffered() noexcept { return timing_type == type::buffered; }
  constexpr bool is_realtime() noexcept { return timing_type == type::realtime; }

  bool supports_dynamic_stream_id{false};
  string client_id;
  uint64_t audio_format{0};
  uint64_t audio_mode{0};
  uint64_t conn_id{0}; // stream connection id
  uint64_t ct{0};      // compression type
  uint64_t spf{0};     // sample frames per packet
  uint8v key;          // shared key (for decipher)

  // timing
  cat timing_cat{cat::unspecified};
  type timing_type{type::none};
  proto timing_proto{proto::none};
};

class Ctx : public std::enable_shared_from_this<Ctx> {

public:
  static auto create() noexcept { return std::shared_ptr<Ctx>(new Ctx()); }
  auto ptr() noexcept { return shared_from_this(); }

  void setup_stream(const auto timing_protocol) noexcept {
    if (timing_protocol == csv{"PTP"}) {
      stream_info.timing_cat = stream_info_t::cat::ptp_stream;
      stream_info.timing_proto = stream_info_t::proto::ptp;

    } else if (timing_protocol == csv{"NTP"}) {
      stream_info.timing_cat = stream_info_t::cat::ptp_stream;
      stream_info.timing_proto = stream_info_t::proto::ntp;

    } else {
      stream_info.timing_cat = stream_info_t::cat::remote_control;
      stream_info.timing_proto = stream_info_t::proto::none;
    }
  }

  template <typename T> T setup_stream_type(T type) noexcept {
    if (type == static_cast<T>(stream_info_t::type::buffered)) {
      stream_info.timing_type = stream_info_t::type::buffered;
    } else if (type == static_cast<T>(stream_info_t::type::realtime)) {
      stream_info.timing_type = stream_info_t::type::realtime;
    } else {
      stream_info.timing_type = stream_info_t::type::none;
    }

    return type;
  }

private:
  Ctx() noexcept : cseq{0}, active_remote{0} {}

public:
  // from RTSP headers
  int64_t cseq;          // i.e. CSeq: 8 (message seq num for active session, increasing from zero)
  int64_t active_remote; // i.e. Active-Remote: 1570223890
  string dacp_id;        // i.e. DACP-ID: DF86B6D21A6C805F
  string user_agent;     // i.e. User-Agent: AirPlay/665.13.1
  int64_t proto_ver;     // i.e. X-Apple-ProtocolVersion: 1
  string client_name;    // i.e. X-Apple-Client-Name: xapham

  string group_id; // airplay group id
  bool group_contains_group_leader{false};

  stream_info_t stream_info;

public:
  static constexpr csv module_id{"RTSP_CTX"};
};

} // namespace rtsp
} // namespace pierre