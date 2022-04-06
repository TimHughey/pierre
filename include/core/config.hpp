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

#include <array>
#include <libconfig.h++>
#include <memory>
#include <string>

namespace pierre {

class Config : public libconfig::Config {
public:
  typedef std::array<char, 37> UUID;
  using string = std::string;

public:
  Config();

  bool findFile(const string &file);
  const string &firmwareVersion() { return firmware_version; };
  bool load();

  void test(const char *setting, const char *key);

public:
  enum DisableStandbyMode { Off = 0, Auto, Always };
  enum PlaybackMode { Stereo = 0, Mono, ReverseStereo, LeftOnly, RightOnly };
  enum SpsFormat {
    UNKNOWN = 0,
    S8,
    U8,
    S16,
    S16_LE,
    S16_BE,
    S24,
    S24_LE,
    S24_BE,
    S24_3LE,
    S24_3BE,
    S32,
    S32_LE,
    S32_BE,
    AUTO,
    INVALID
  };

  enum VolumeControlProfile { Standard = 0, Flat };

public:
  // used for AirPlay mDNS service registration
  string firmware_version;

  // wait this long before asking for a missing packet to be resent
  const double resend_control_first_check_time = 0.10;
  // wait this long between making requests
  const double resend_control_check_interval_time = 0.25;

  // if the packet is missing this close to the time of use, give up
  const double resend_control_last_check_time = 0.10;

  // leave approximately one second's worth of buffers
  // free after calculating the effective latency.
  // e.g. if we have 1024 buffers or 352 frames = 8.17 seconds and we have a nominal latency of 2.0
  // seconds then we can add an offset of 5.17 seconds and still leave a second's worth of buffers
  // for unexpected circumstances

  // when effective latency is calculated, ensure this number of buffers are unallocated
  static constexpr auto minimum_free_buffer_headroom = 125;

  pthread_mutex_t lock;

  // normally the app is called shairport-syn, but it may be symlinked
  char *appName;
  const char *password = "";

  // the name for the shairport service, e.g. "Shairport
  // Sync Version %v running on host %h"
  static constexpr auto service_name = "Jophiel";

  // only needs 6 but 8 is handy when converting this to a number
  uint8_t hw_addr[8];
  string mac_addr;
  const int port = 7000;
  const int udp_port_base = 6000;
  const int udp_port_range = 10;
  const bool ignore_volume_control = true;

  const bool allow_session_interruption = true;

  // while in play mode, exit if no packets of audio come in for
  // more than this number of seconds . Zero means never exit.
  const int timeout = 0;

  // this is used to maintain backward compatibility
  // with the old -t option behaviour; only set by -t 0,
  // cleared by everything else
  const bool dont_check_timeout = true;

  char *mdns_name;

  int buffer_start_fill;

  int daemonise;
  int daemonise_store_pid; // don't try to save a PID file
  char *piddir;
  char *computed_piddir; // the actual pid directory to create, if any
  char *pidfile;

  // file descriptor of the file or pipe to log stuff to.
  int log_fd;

  // path to file or pipe to log to, if any
  char *log_file_path;

  // log output level
  int logOutputLevel;
  int debugger_show_elapsed_time;  // in the debug message, display the time
                                   // since startup
  int debugger_show_relative_time; // in the debug message, display the time
                                   // since the last one
  int debugger_show_file_and_line; // in the debug message, display the filename
                                   // and line number
  int statistics_requested;
  const PlaybackMode playback_mode = Mono;

  // The regtype is the service type followed by the protocol,
  // separated by a dot, by default “_raop._tcp.” for AirPlay 2.
  const char *regtype2 = "_raop._tcp";

  // a string containg the interface name, or NULL if nothing specified
  const char *interface = nullptr;

  // this will be the length in seconds of the audio backend buffer -- the
  // DAC buffer for ALSA
  double audio_backend_buffer_desired_length;

  // below this, soxr interpolation will not occur -- it'll be
  // basic interpolation instead.
  double audio_backend_buffer_interpolation_threshold_in_seconds;

  // this will be the offset in seconds to
  // compensate for any fixed latency there
  // might be in the audio path
  double audio_backend_latency_offset;

  // true if the lead-in time should
  // be from as soon as packets are
  // received
  int audio_backend_silent_lead_in_time_auto;

  // the length of the silence that
  // should precede a play.
  double audio_backend_silent_lead_in_time;

  double active_state_timeout; // the amount of time from when play ends to when
                               // the system leaves into the "active" mode.

  SpsFormat output_format = S16_LE;
  const uint32_t output_rate = 44100;

  DisableStandbyMode disable_standby_mode;

  // a linked list of the clock gradients discovered for all
  // DACP IDs can't use IP numbers as they might be given to
  // different devices can't get hold of MAC addresses. can't
  // define the nvll linked list struct here
  void *gradients;

  // the features code is a 64-bit number, but in the mDNS advertisement, the least significant 32
  // bit are given first for example, if the features number is 0x1C340405F4A00, it will be given as
  // features=0x405F4A00,0x1C340 in the mDNS string, and in a signed decimal number in the plist:
  // 496155702020608 this setting here is the source of both the plist features response and the
  // mDNS string.

  // const uint64_t _one = 1;
  // const uint64_t _mask = (_one << 17) | (_one << 16) | (_one << 15) | (_one << 50);

  // APX + Authentication4 (b14) with no metadata (see below)
  // const uint64_t airplay_features = 0x1C340405D4A00 & (~_mask);
  // std::array _feature_bits{9, 11, 18, 19, 30, 40, 41, 51};
  // uint64_t airplay_features = 0x00;
  const uint64_t airplay_features = 0x1C340445F8A00;

  // Advertised with mDNS and returned with GET /info, see
  // https://openairplay.github.io/airplay-spec/status_flags.html
  // 0x4: Audio cable attached, no PIN required (transient pairing)
  // 0x204: Audio cable attached, OneTimePairingRequired
  // 0x604: Audio cable attached, OneTimePairingRequired, device setup for Homekit access control
  const uint32_t airplay_statusflags = 0x04;
  double airplay_volume = -24.0;
  // used by airplay
  const uint32_t fixedLatencyOffset = 11025; // this sounds like it works properly.

  // for the Bonjour advertisement and the GETINFO PList
  string airplay_device_id;

  // non-NULL, 4 char PIN, if required for pairing
  char *airplay_pin;

  // UUID in the Bonjour advertisement and the GETINFO Plist
  UUID airplay_pi{0};

  // client name for nqptp service
  char *nqptp_shared_memory_interface_name;

private:
  string _cfg_file;
  string _dmx_host;
};

typedef std::shared_ptr<Config> ConfigPtr;

} // namespace pierre