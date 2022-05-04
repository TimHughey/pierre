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

#include <arpa/inet.h>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <openssl/aes.h>
#include <pthread.h>
#include <source_location>
#include <string>
#include <sys/socket.h>

#include "decouple/flush_request.hpp"
#include "decouple/ping_record.hpp"
#include "decouple/stream.hpp"

namespace pierre {

typedef uint32_t SeqNum;

// shared pointer forward decl
class ConnInfo;

typedef std::shared_ptr<ConnInfo> shConnInfo;

class ConnInfo : public std::enable_shared_from_this<ConnInfo> {
private:
  using src_loc = std::source_location;
  using string = std::string;
  using string_view = std::string_view;

  using Mutex = std::mutex;
  using CondV = std::condition_variable;

  typedef const char *ccs;
  typedef const string &csr;
  typedef const string_view csv;
  typedef std::vector<unsigned char> SessionKey;

public:
  [[nodiscard]] static shConnInfo inst();
  static void reset(const src_loc loc = src_loc::current());

  // getters
  const SessionKey &sessionKey() const { return session_key; }

  // setters
  void saveSessionKey(csr key);

  enum clock_status_t : uint8_t {
    clock_no_anchor_info = 0,
    clock_ok,
    clock_service_unavailable,
    clock_access_error,
    clock_data_unavailable,
    clock_no_master,
    clock_version_mismatch,
    clock_not_synchronised,
    clock_not_valid,
    clock_not_ready,
  };

  enum airplay_stream_c : uint8_t { // "c" for category
    unspecified_stream_category = 0,
    ptp_stream,
    ntp_stream,
    remote_control_stream
  };

  enum timing_t : uint8_t { ts_ntp = 0, ts_ptp };
  enum airplay_t : uint8_t { ap_1, ap_2 };
  enum airplay_stream_t : uint8_t { realtime_stream, buffered_stream };

  static constexpr auto BUFFER_FRAMES = 1024;
  //  2^7 is 128. At 1 per three seconds; approximately six minutes of records
  static constexpr auto ping_history = (1 << 7);

  string UserAgent;   // free this on teardown
  int AirPlayVersion; // zero if not an AirPlay session. Used to help calculate
                      // latency
  int latency_warning_issued;
  uint32_t latency; // the actual latency used for this play session

  string auth_nonce; // the session nonce, if needed
  Stream stream;
  volatile int stop;
  volatile int running;
  volatile uint64_t watchdog_bark_time;
  volatile int watchdog_barks;  // number of times the watchdog has timed out and
                                // done something
  int unfixable_error_reported; // set when an unfixable error command has been
                                // executed.

  time_t playstart;
  pthread_t thread, timer_requester, rtp_audio_thread, rtp_control_thread, rtp_timing_thread,
      player_watchdog_thread;

  // buffers to delete on exit
  int32_t *tbuf;
  int32_t *sbuf;
  char *outbuf;

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
  pthread_t *player_thread;

  int input_bytes_per_frame, output_bytes_per_frame, output_sample_ratio;
  int max_frame_size_change;
  int64_t previous_random_number;
  // alac_file *decoder_info;
  uint64_t packet_count;
  uint64_t packet_count_since_flush;
  int connection_state_to_output;
  uint64_t first_packet_time_to_play;
  int64_t time_since_play_started; // nanoseconds
                                   // stats
  uint64_t missing_packets, late_packets, too_late_packets, resend_requests;
  int decoder_in_use;
  // debug variables
  int32_t last_seqno_read;
  // mutexes and condition variables
  pthread_cond_t flowcontrol;
  pthread_mutex_t ab_mutex, flush_mutex, volume_control_mutex;

  int fix_volume;
  double initial_airplay_volume;
  int initial_airplay_volume_set;

  uint32_t timestamp_epoch, last_timestamp,
      maximum_timestamp_interval; // timestamp_epoch of zero means not
                                  // initialised, could start at 2 or 1.
  int ab_buffering, ab_synced;
  int64_t first_packet_timestamp;
  int flush_requested;
  int flush_output_flushed; // true if the output device has been flushed.
  uint32_t flush_rtp_timestamp;
  uint64_t time_of_last_audio_packet;
  SeqNum ab_read, ab_write;
  AES_KEY aes;

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
  uint16_t client_rtsp_port;
  string self_ip_string;   // the ip string being used by this
                           // program -- it
  uint16_t self_rtsp_port; // could be one of many, so we need to know it

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

  airplay_stream_c airplay_stream_category; // is it a remote control stream or a normal
                                            // "full service" stream? (will be unspecified if
                                            // not build for AirPlay 2)

  string airplay_gid;                   // UUID in the Bonjour advertisement -- if NULL, the group
                                        // UUID is the same as the pi UUID
  airplay_t airplay_type;               // are we using AirPlay 1 or AirPlay 2 protocol on
                                        // this connection?
  airplay_stream_t airplay_stream_type; // is it realtime audio or buffered audio...
  timing_t timing_type;                 // are we using NTP or PTP on this connection?

  pthread_t *rtp_event_thread;
  pthread_t rtp_ap2_control_thread;
  pthread_t rtp_realtime_audio_thread;
  pthread_t rtp_buffered_audio_thread;

  int last_anchor_info_is_valid;
  uint32_t last_anchor_rtptime;
  uint64_t last_anchor_local_time;
  uint64_t last_anchor_time_of_update;
  uint64_t last_anchor_validity_start_time;

  ssize_t ap2_audio_buffer_size;
  ssize_t ap2_audio_buffer_minimum_size;

  // flush requests (when not null), mutex protected
  FlushList flush_requests;
  int ap2_flush_requested;
  int ap2_flush_from_valid;
  uint32_t ap2_flush_from_rtp_timestamp;
  uint32_t ap2_flush_from_sequence_number;
  uint32_t ap2_flush_until_rtp_timestamp;
  uint32_t ap2_flush_until_sequence_number;
  int ap2_rate;         // protect with flush mutex, 0 means don't play, 1 means play
  int ap2_play_enabled; // protect with flush mutex

  // ap2_pairing ap2_control_pairing;

  int event_socket;
  sockaddr_storage ap2_remote_control_socket_addr; // a socket pointing to the
                                                   // control port of the client
  socklen_t ap2_remote_control_socket_addr_length;
  int ap2_control_socket;
  int realtime_audio_socket;
  int buffered_audio_socket;

  uint16_t local_event_port;
  uint16_t local_ap2_control_port;
  uint16_t local_realtime_audio_port;
  uint16_t local_buffered_audio_port;

  uint64_t audio_format;
  uint64_t compression;
  SessionKey session_key; // needs to be free'd at the end
  uint64_t frames_packet;
  uint64_t type;
  uint64_t networkTimeTimelineID;   // the clock ID used by the player
  uint8_t groupContainsGroupLeader; // information coming from the SETUP

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

  int time_ping_count;
  PingRecord time_pings[ping_history];

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

  int play_number_after_flush;

  // remote control stuff. The port to which to send commands is not specified,
  // so you have to use mdns to find it. at present, only avahi can do this

  string dacp_id; // id of the client -- used to find the port to be used
  //  uint16_t dacp_port;          // port on the client to send remote control
  //  messages to, else zero
  string dacp_active_remote;   // key to send to the remote controller
  string dapo_private_storage; // this is used for compatibility, if dacp stuff
                               // isn't enabled.

  int enable_dither; // needed for filling silences before play actually starts
  uint64_t dac_buffer_queue_minimum_length;

public:
  static ccs fnName(const src_loc loc = src_loc::current()) { return loc.function_name(); }

private:
  ConnInfo();

private:
  static shConnInfo __inst__;
};

} // namespace pierre