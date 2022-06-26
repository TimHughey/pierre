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

#include "base/typical.hpp"

#include <array>
#include <libconfig.h++>
#include <memory>
#include <string>

namespace pierre {

class Config : public libconfig::Config {
private:
  struct Inject {
    csr app_name;
    csr cli_cfg_file;
  };

public:
  Config(const Inject &di);

  csr appName() const { return _app_name; }
  bool findFile();
  csr firmwareVersion() const { return _firmware_vsn; };
  csr serviceName() const { return _service_name; }
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
  // wait this long before asking for a missing packet to be resent
  const double resend_control_first_check_time = 0.10;
  // wait this long between making requests
  const double resend_control_check_interval_time = 0.25;

  // if the packet is missing this close to the time of use, give up
  const double resend_control_last_check_time = 0.10;

  // leave approximately one second's worth of buffers
  // free after calculating the effective latency.
  // e.g. if we have 1024 buffers or 352 frames = 8.17 seconds and we have a
  // nominal latency of 2.0 seconds then we can add an offset of 5.17 seconds
  // and still leave a second's worth of buffers for unexpected circumstances

  // when effective latency is calculated, ensure this number of buffers are
  // unallocated
  static constexpr auto minimum_free_buffer_headroom = 125;

  pthread_mutex_t lock;

  // only needs 6 but 8 is handy when converting this to a number
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

  double airplay_volume = -24.0;
  // used by airplay
  const uint32_t fixedLatencyOffset = 11025; // this sounds like it works properly.

private:
  csr _app_name;
  string _cfg_file;
  string _cli_cfg_file;
  string _dmx_host;
  // used for AirPlay mDNS service registration
  string _firmware_vsn;

  // NOTE: hardcoded!!!
  string _service_name{"Jophiel"};
};

} // namespace pierre