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

  static constexpr auto BUFFER_FRAMES = 1024;
  //  2^7 is 128. At 1 per three seconds; approximately six minutes of records
  static constexpr auto ping_history = (1 << 7);

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
  string UserAgent;   // free this on teardown
  int AirPlayVersion; // zero if not an AirPlay session. Used to help calculate
                      // latency
  int latency_warning_issued;
  uint32_t latency; // the actual latency used for this play session

  // Stream stream;
  volatile int stop;
  volatile int running;
  volatile uint64_t watchdog_bark_time;
  volatile int watchdog_barks;  // number of times the watchdog has timed out and
                                // done something
  int unfixable_error_reported; // set when an unfixable error command has been
                                // executed.

  time_t playstart;

  // for holding the output rate information until printed out at the end of a
  // session
  double raw_frame_rate;
  double corrected_frame_rate;
  int frame_rate_valid;

  // for holding input rate information until printed out at the end of a
  // session

  double input_frame_rate;
  int input_frame_rate_starting_point_is_valid;

  uint64_t frames_inward_measurement_start_time;
  uint32_t frames_inward_frames_received_at_measurement_start_time;

  uint64_t frames_inward_measurement_time;
  uint32_t frames_inward_frames_received_at_measurement_time;

  // other stuff...

  int input_bytes_per_frame, output_bytes_per_frame, output_sample_ratio;
  int max_frame_size_change;
  int64_t previous_random_number;
  uint64_t packet_count;
  int connection_state_to_output;
  uint64_t first_packet_time_to_play;
  int64_t time_since_play_started; // nanoseconds
                                   // stats
  uint64_t missing_packets, late_packets, too_late_packets, resend_requests;
  // debug variables
  int32_t last_seqno_read;
  // mutexes and condition variables

  double initial_airplay_volume;
  int initial_airplay_volume_set;

  uint32_t timestamp_epoch, last_timestamp,
      maximum_timestamp_interval; // timestamp_epoch of zero means not
                                  // initialised, could start at 2 or 1.
  int ab_buffering, ab_synced;
  int64_t first_packet_timestamp;
  uint64_t time_of_last_audio_packet;
  SeqNum ab_read, ab_write;
  //  AES_KEY aes;

  int amountStuffed;

  int32_t framesProcessedInThisEpoch;
  int32_t framesGeneratedInThisEpoch;
  int32_t correctionsRequestedInThisEpoch;
  int64_t syncErrorsInThisEpoch;

  // RTP stuff
  // only one RTP session can be active at a time.
  int rtp_running;
  uint64_t rtp_time_of_last_resend_request_error_ns;

  string client_ip_string; // the ip string pointing to the
                           // client

  uint32_t self_scope_id;       // if it's an ipv6 connection, this will be its scope
  int16_t connection_ip_family; // AF_INET / AF_INET6

  sockaddr_storage rtp_client_control_socket; // a socket pointing to the
                                              // control port of the client
  sockaddr_storage rtp_client_timing_socket;  // a socket pointing to the timing
                                              // port of the client
  int audio_socket;                           // our local [server] audio socket
  int control_socket;                         // our local [server] control socket
  int timing_socket;                          // local timing socket

  uint16_t remote_control_port;
  uint16_t remote_timing_port;
  uint16_t local_audio_port;
  uint16_t local_control_port;
  uint16_t local_timing_port;

  int64_t latency_delayed_timestamp; // this is for debugging only...

  // this is what connects an rtp timestamp to the remote time

  int anchor_remote_info_is_valid;
  int anchor_clock_is_new;

  // these can be modified if the master clock changes over time
  uint64_t anchor_clock;
  uint64_t anchor_time; // this is the time according to the clock
  uint32_t anchor_rtptime;

  // these are used to identify when the master clock becomes equal to the
  // actual anchor clock information, so it can be used to avoid accumulating
  // errors
  uint64_t actual_anchor_clock;
  uint64_t actual_anchor_time;
  uint32_t actual_anchor_rtptime;

  clock_status_t clock_status;

  // are we using NTP or PTP on this connection?
  timing_t timing_type;

  int last_anchor_info_is_valid;
  uint32_t last_anchor_rtptime;
  uint64_t last_anchor_local_time;
  uint64_t last_anchor_time_of_update;
  uint64_t last_anchor_validity_start_time;

  ssize_t ap2_audio_buffer_size;
  ssize_t ap2_audio_buffer_minimum_size;

  int ap2_rate;
  int ap2_play_enabled;

  // ap2_pairing ap2_control_pairing;

  // uint64_t audio_format;
  // uint64_t compression;
  // SessionKey session_key; // needs to be free'd at the end
  // uint64_t frames_packet;
  // uint64_t type;
  uint64_t networkTimeTimelineID; // the clock ID used by the player

  // used as the initials values for calculating the rate at which the source
  // thinks it's sending frames
  uint32_t initial_reference_timestamp;
  uint64_t initial_reference_time;
  double remote_frame_rate;

  // the ratio of the following should give us the operating rate, nominally
  // 44,100
  int64_t reference_to_previous_frame_difference;
  uint64_t reference_to_previous_time_difference;

  // debug variables
  int request_sent;

  // int time_ping_count;
  // PingRecord time_pings[ping_history];

  uint64_t departure_time; // dangerous -- this assumes that there will never be
                           // two timing request in flight at the same time

  pthread_mutex_t reference_time_mutex;
  pthread_mutex_t watchdog_mutex;

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

  int last_stuff_request;

  // int64_t play_segment_reference_frame;
  // uint64_t play_segment_reference_frame_remote_time;

  // allow it to be negative because seq_diff may be negative
  int32_t buffer_occupancy;
  int64_t session_corrections;

  // remote control stuff. The port to which to send commands is not specified,
  // so you have to use mdns to find it. at present, only avahi can do this

  string dacp_id; // id of the client -- used to find the port to be used
  //  uint16_t dacp_port;          // port on the client to send remote control
  //  messages to, else zero

  string dapo_private_storage; // this is used for compatibility, if dacp stuff
                               // isn't enabled.

  int enable_dither; // needed for filling silences before play actually starts
  uint64_t dac_buffer_queue_minimum_length;
};
} // namespace airplay

} // namespace pierre