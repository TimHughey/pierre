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

#include "base/typical.hpp"
#include "common/ops_mode.hpp"
#include "common/typedefs.hpp"
#include "conn_info/stream.hpp"
#include "conn_info/stream_info.hpp"

#include <arpa/inet.h>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <pthread.h>
#include <string>
#include <sys/socket.h>
#include <unordered_map>
#include <vector>

namespace pierre {
namespace airplay {

typedef std::vector<unsigned char> SessionKey;

class ConnInfo;
typedef std::shared_ptr<ConnInfo> shConnInfo;

namespace shared {
std::optional<shConnInfo> &connInfo();
} // namespace shared

class ConnInfo : public std::enable_shared_from_this<ConnInfo> {
private:
  using Mutex = std::mutex;
  using CondV = std::condition_variable;

public:
  static shConnInfo init() { return shared::connInfo().emplace(new ConnInfo()); }
  static shConnInfo ptr() { return shared::connInfo().value()->shared_from_this(); }
  static void reset() { shared::connInfo().reset(); }

private:
  ConnInfo() {} // all access through shared ptr

public:
  static constexpr size_t bufferSize() { return 1024 * 1024 * 8; };
  const StreamData &streamData() const { return stream_info.data(); }

  // getters
  csv sessionKey() const { return stream_info.key(); }

  // setters
  void save(const Stream &new_stream) { stream = new_stream; }
  void save(const StreamData &data);

  void sessionKeyClear() { stream_info.keyClear(); }

  StreamInfo stream_info;
  Stream stream;

  // stream category, stream type and timing type
  // is it a remote control stream or a normal "full service" stream?
  airplay_stream_c airplay_stream_category = airplay_stream_c::unspecified_stream_category;
  // are we using AirPlay 1 or AirPlay 2 protocol on this connection?
  airplay_t airplay_type = airplay_t::ap_2; // Always AirPlay2
  // is it realtime audio or buffered audio?
  airplay_stream_t airplay_stream_type;

  // captured from RTSP SETUP initial message (no stream data)
  string airplay_gid; // UUID in the Bonjour advertisement -- if NULL, the group
                      // UUID is the same as the pi UUID

  // captured from RTSP SETUP (no stream data)
  bool groupContainsGroupLeader = false;
  csv ops_mode = ops_mode::INITIALIZE;

  string dacp_active_remote; // key to send to the remote controller

private:
  string UserAgent; // free this on teardown

  // for holding the output rate information until printed out at the end of a
  // session
  double raw_frame_rate;
  double corrected_frame_rate;
  int frame_rate_valid;

  // this is what connects an rtp timestamp to the remote time

  int anchor_remote_info_is_valid;
  int anchor_clock_is_new;

  // these can be modified if the master clock changes over time
  uint64_t anchor_clock;
  uint64_t anchor_time; // this is the time according to the clock
  uint32_t anchor_rtptime;

  // are we using NTP or PTP on this connection?
  timing_t timing_type;

  int last_anchor_info_is_valid;
  uint32_t last_anchor_rtptime;
  uint64_t last_anchor_local_time;
  uint64_t last_anchor_time_of_update;
  uint64_t last_anchor_validity_start_time;

  // used as the initials values for calculating the rate at which the source
  // thinks it's sending frames
  uint32_t initial_reference_timestamp;
  uint64_t initial_reference_time;
  double remote_frame_rate;

  // the ratio of the following should give us the operating rate, nominally
  // 44,100
  int64_t reference_to_previous_frame_difference;
  uint64_t reference_to_previous_time_difference;

  // if no drift, this would be exactly 1.0;
  // likely it's slightly above or  below.
  double local_to_remote_time_gradient;

  // the number of samples used
  // to calculate the gradient
  int local_to_remote_time_gradient_sample_count;

  // add the following to the local time to get the remote time modulo 2^64
  // used to switch between local and remote clocks
  uint64_t local_to_remote_time_difference;

  // when the above was calculated
  uint64_t local_to_remote_time_difference_measurement_time;

  // remote control stuff. The port to which to send commands is not specified,
  // so you have to use mdns to find it. at present, only avahi can do this

  string dacp_id; // id of the client -- used to find the port to be used
  //  uint16_t dacp_port;          // port on the client to send remote control
  //  messages to, else zero

  int enable_dither; // needed for filling silences before play actually starts
  uint64_t dac_buffer_queue_minimum_length;
};
} // namespace airplay

} // namespace pierre