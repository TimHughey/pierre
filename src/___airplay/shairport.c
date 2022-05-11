/*
 * Shairport, an Apple Airplay receiver
 * Copyright (c) James Laird 2013
 * All rights reserved.
 * Modifications and additions (c) Mike Brady 2014--2022
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libconfig.h>
#include <libgen.h>
#include <memory.h>
#include <net/if.h>
#include <popt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "config.h"

#include "ptp-utilities.h"
#include <gcrypt.h>
#include <sodium.h>
#include <uuid/uuid.h>

#include <openssl/md5.h>

#include "activity_monitor.h"
#include "audio.h"
#include "common.h"
#include "rtp.h"
#include "rtsp.h"

#include <libdaemon/dexec.h>
#include <libdaemon/dfork.h>
#include <libdaemon/dlog.h>
#include <libdaemon/dpid.h>
#include <libdaemon/dsignal.h>

pid_t pid;
int this_is_the_daemon_process = 0;

#ifndef UUID_STR_LEN
#define UUID_STR_LEN 36
#endif

#define strnull(s) ((s) ? (s) : "(null)")

pthread_t rtsp_listener_thread;

int killOption = 0;
int daemonisewith = 0;
int daemonisewithout = 0;

// static int shutting_down = 0;
char configuration_file_path[4096 + 1];
char actual_configuration_file_path[4096 + 1];

char first_backend_name[256];

void print_version(void) {
  char *version_string = get_version_string();
  if (version_string) {
    printf("%s\n", version_string);
    free(version_string);
  } else {
    debug(1, "Can't print version string!");
  }
}

void usage(char *progname) {
  printf("Usage: %s [options...]\n", progname);
  printf("  or:  %s [options...] -- [audio output-specific options]\n",
         progname);
  printf("\n");
  printf("Options:\n");
  printf("    -h, --help              show this help.\n");

  printf("    -d, --daemon            daemonise.\n");
  printf("    -j, --justDaemoniseNoPIDFile            daemonise without a PID "
         "file.\n");
  printf("    -k, --kill              kill the existing shairport daemon.\n");
  printf("    -V, --version           show version information.\n");
  printf("    -c, --configfile=FILE   read configuration settings from FILE. "
         "Default is "
         "/etc/shairport-sync.conf.\n");

  printf("\n");
  printf("The following general options are for backward compatibility. These "
         "and all new options "
         "have settings in the configuration file, by default "
         "/etc/shairport-sync.conf:\n");
  printf("    -v, --verbose           -v print debug information; -vv more; "
         "-vvv lots.\n");
  printf("    -p, --port=PORT         set RTSP listening port.\n");
  printf("    -a, --name=NAME         set advertised name.\n");
  printf("    -L, --latency=FRAMES    [Deprecated] Set the latency for audio "
         "sent from an unknown "
         "device.\n");
  printf(
      "                            The default is to set it automatically.\n");
  printf("    -S, --stuffing=MODE set how to adjust current latency to match "
         "desired latency, "
         "where \n");
  printf("                            \"basic\" inserts or deletes audio "
         "frames from "
         "packet frames with low processor overhead, and \n");
  printf("                            \"soxr\" uses libsoxr to minimally "
         "resample packet frames -- "
         "moderate processor overhead.\n");
  printf("                            \"soxr\" option only available if built "
         "with soxr support.\n");
  printf("    -B, --on-start=PROGRAM  run PROGRAM when playback is about to "
         "begin.\n");
  printf("    -E, --on-stop=PROGRAM   run PROGRAM when playback has ended.\n");
  printf("                            For -B and -E options, specify the full "
         "path to the program, "
         "e.g. /usr/bin/logger.\n");
  printf("                            Executable scripts work, but must have "
         "the appropriate shebang "
         "(#!/bin/sh) in the headline.\n");
  printf("    -w, --wait-cmd          wait until the -B or -E programs finish "
         "before continuing.\n");
  printf("    -o, --output=BACKEND    select audio output method.\n");
  printf("    -m, --mdns=BACKEND      force the use of BACKEND to advertize "
         "the service.\n");
  printf("                            if no mdns provider is specified,\n");
  printf("                            shairport tries them all until one "
         "works.\n");
  printf("    -r, --resync=THRESHOLD  [Deprecated] resync if error exceeds "
         "this number of frames. "
         "Set to 0 to "
         "stop resyncing.\n");
  printf("    -t, --timeout=SECONDS   go back to idle mode from play mode "
         "after a break in "
         "communications of this many seconds (default 120). Set to 0 never to "
         "exit play mode.\n");
  printf("    --statistics            print some interesting statistics -- "
         "output to the logfile "
         "if running as a daemon.\n");
  printf("    --tolerance=TOLERANCE   [Deprecated] allow a synchronization "
         "error of TOLERANCE "
         "frames (default "
         "88) before trying to correct it.\n");
  printf("    --password=PASSWORD     require PASSWORD to connect. Default is "
         "not to require a "
         "password.\n");
  printf("    --logOutputLevel        log the output level setting -- useful "
         "for setting maximum "
         "volume.\n");
  printf("    -u, --use-stderr        log messages through STDERR rather than "
         "the system log.\n");
  printf("\n");
  mdns_ls_backends();
  printf("\n");
  audio_ls_outputs();
}

int parse_options(int argc, char **argv) {
  // there are potential memory leaks here -- it's called a second time,
  // previously allocated strings will dangle.
  char *raw_service_name =
      NULL; /* Used to pick up the service name before possibly expanding it */
  char *stuffing = NULL; /* used for picking up the stuffing option */
  signed char c;         /* used for argument parsing */
  // int i = 0;                     /* used for tracking options */
  int fResyncthreshold = (int)(config.resyncthreshold * 44100);
  int fTolerance = (int)(config.tolerance * 44100);
  poptContext optCon; /* context for parsing command-line options */
  struct poptOption optionsTable[] = {
      {"verbose", 'v', POPT_ARG_NONE, NULL, 'v', NULL, NULL},
      {"kill", 'k', POPT_ARG_NONE, &killOption, 0, NULL, NULL},
      {"daemon", 'd', POPT_ARG_NONE, &daemonisewith, 0, NULL, NULL},
      {"justDaemoniseNoPIDFile", 'j', POPT_ARG_NONE, &daemonisewithout, 0, NULL,
       NULL},
      {"configfile", 'c', POPT_ARG_STRING, &config.configfile, 0, NULL, NULL},
      {"statistics", 0, POPT_ARG_NONE, &config.statistics_requested, 0, NULL,
       NULL},
      {"logOutputLevel", 0, POPT_ARG_NONE, &config.logOutputLevel, 0, NULL,
       NULL},
      {"version", 'V', POPT_ARG_NONE, NULL, 0, NULL, NULL},
      {"port", 'p', POPT_ARG_INT, &config.port, 0, NULL, NULL},
      {"name", 'a', POPT_ARG_STRING, &raw_service_name, 0, NULL, NULL},
      {"output", 'o', POPT_ARG_STRING, &config.output_name, 0, NULL, NULL},
      {"mdns", 'm', POPT_ARG_STRING, &config.mdns_name, 0, NULL, NULL},
      {"stuffing", 'S', POPT_ARG_STRING, &stuffing, 'S', NULL, NULL},
      {"resync", 'r', POPT_ARG_INT, &fResyncthreshold, 0, NULL, NULL},
      {"timeout", 't', POPT_ARG_INT, &config.timeout, 't', NULL, NULL},
      {"password", 0, POPT_ARG_STRING, &config.password, 0, NULL, NULL},
      {"tolerance", 'z', POPT_ARG_INT, &fTolerance, 0, NULL, NULL},
      {"use-stderr", 'u', POPT_ARG_NONE, NULL, 'u', NULL, NULL},

      POPT_AUTOHELP{NULL, 0, 0, NULL, 0, NULL, NULL}};

  // we have to parse the command line arguments to look for a config file
  int optind;
  optind = argc;
  int j;
  for (j = 0; j < argc; j++)
    if (strcmp(argv[j], "--") == 0)
      optind = j;

  optCon = poptGetContext(NULL, optind, (const char **)argv, optionsTable, 0);
  if (optCon == NULL)
    die("Can not get a secondary popt context.");
  poptSetOtherOptionHelp(optCon, "[OPTIONS]* ");

  /* Now do options processing just to get a debug level */
  debuglev = 0;
  while ((c = poptGetNextOpt(optCon)) >= 0) {
    switch (c) {
    case 'v':
      debuglev++;
      break;
    case 'u':
      log_to_stderr();
      break;
    case 'D':
      inform("Warning: the option -D or --disconnectFromOutput is deprecated.");
      break;
    case 'R':
      inform("Warning: the option -R or --reconnectToOutput is deprecated.");
      break;
    case 'A':
      inform("Warning: the option -A or --AirPlayLatency is deprecated and "
             "ignored. This setting "
             "is now "
             "automatically received from the AirPlay device.");
      break;
    case 'i':
      inform("Warning: the option -i or --iTunesLatency is deprecated and "
             "ignored. This setting is "
             "now "
             "automatically received from iTunes");
      break;
    case 'f':
      inform("Warning: the option --forkedDaapdLatency is deprecated and "
             "ignored. This setting is now "
             "automatically received from forkedDaapd");
      break;
    case 'r':
      inform("Warning: the option -r or --resync is deprecated. Please use the "
             "\"resync_threshold_in_seconds\" setting in the config file "
             "instead.");
      break;
    case 'z':
      inform(
          "Warning: the option --tolerance is deprecated. Please use the "
          "\"drift_tolerance_in_seconds\" setting in the config file instead.");
      break;
    }
  }
  if (c < -1) {
    die("%s: %s", poptBadOption(optCon, POPT_BADOPTION_NOALIAS),
        poptStrerror(c));
  }

  poptFreeContext(optCon);

  if ((daemonisewith) && (daemonisewithout))
    die("Select either daemonize_with_pid_file or daemonize_without_pid_file "
        "-- you have selected "
        "both!");
  if ((daemonisewith) || (daemonisewithout)) {
    config.daemonise = 1;
    if (daemonisewith)
      config.daemonise_store_pid = 1;
  };

  config.resyncthreshold = 1.0 * fResyncthreshold / 44100;
  config.tolerance = 1.0 * fTolerance / 44100;
  config.audio_backend_silent_lead_in_time_auto =
      1; // start outputting silence as soon as packets start arriving
  config.airplay_volume =
      -24.0; // if no volume is ever set, default to initial default value if
             // nothing else comes in first.
  config.fixedLatencyOffset = 11025; // this sounds like it works properly.
  config.active_state_timeout = 10.0;

  config.volume_range_hw_priority =
      0; // if combining software and hardware volume control, give the software
         // priority i.e. when reducing volume, reduce the sw first before
         // reducing the software. this is because some hw mixers mute at the
         // bottom of their range, and they don't always
  // advertise this fact
  config.resend_control_first_check_time =
      0.10; // wait this many seconds before requesting the resending of a
            // missing packet
  config.resend_control_check_interval_time =
      0.25; // wait this many seconds before again requesting the resending of a
            // missing packet
  config.resend_control_last_check_time =
      0.10; // give up if the packet is still missing this close to when it's
            // needed

  config.minimum_free_buffer_headroom =
      125; // leave approximately one second's worth of buffers
           // free after calculating the effective latency.
  // e.g. if we have 1024 buffers or 352 frames = 8.17 seconds and we have a
  // nominal latency of 2.0 seconds then we can add an offset of 5.17 seconds
  // and still leave a second's worth of buffers for unexpected circumstances

  config.loudness_reference_volume_db = -20;

  // the features code is a 64-bit number, but in the mDNS advertisement, the
  // least significant 32 bit are given first for example, if the features
  // number is 0x1C340405F4A00, it will be given as features=0x405F4A00,0x1C340
  // in the mDNS string, and in a signed decimal number in the plist:
  // 496155702020608 this setting here is the source of both the plist features
  // response and the mDNS string. note: 0x300401F4A00 works but with weird
  // delays and stuff config.airplay_features = 0x1C340405FCA00;
  uint64_t mask = ((uint64_t)1 << 17) | ((uint64_t)1 << 16) |
                  ((uint64_t)1 << 15) | ((uint64_t)1 << 50);
  config.airplay_features =
      0x1C340405D4A00 &
      (~mask); // APX + Authentication4 (b14) with no metadata (see below)
  // Advertised with mDNS and returned with GET /info, see
  // https://openairplay.github.io/airplay-spec/status_flags.html 0x4: Audio
  // cable attached, no PIN required (transient pairing), 0x204: Audio cable
  // attached, OneTimePairingRequired 0x604: Audio cable attached,
  // OneTimePairingRequired, device was setup for Homekit access control
  config.airplay_statusflags = 0x04;
  // Set to NULL to work with transient pairing
  config.airplay_pin = NULL;

  // use the start of the config.hw_addr and the PID to generate the default
  // airplay_device_id
  uint64_t temporary_airplay_id = nctoh64(config.hw_addr);
  temporary_airplay_id =
      temporary_airplay_id >>
      16; // we only use the first 6 bytes but have imported 8.

  // now generate a UUID
  // from
  // https://stackoverflow.com/questions/51053568/generating-a-random-uuid-in-c
  // with thanks
  uuid_t binuuid;
  uuid_generate_random(binuuid);

  char *uuid = malloc(UUID_STR_LEN);
  // Produces a UUID string at uuid consisting of lower-case letters
  uuid_unparse_lower(binuuid, uuid);
  config.airplay_pi = uuid;

  // config_setting_t *setting;
  const char *str = 0;
  int value = 0;
  double dvalue = 0.0;

  // debug(1, "Looking for the configuration file \"%s\".", config.configfile);

  config_init(&config_file_stuff);

  char *config_file_real_path = realpath(config.configfile, NULL);
  if (config_file_real_path == NULL) {
    debug(2, "can't resolve the configuration file \"%s\".", config.configfile);
  } else {
    debug(2, "looking for configuration file at full path \"%s\"",
          config_file_real_path);
    /* Read the file. If there is an error, report it and exit. */
    if (config_read_file(&config_file_stuff, config_file_real_path)) {
      free(config_file_real_path);
      config_set_auto_convert(
          &config_file_stuff,
          1); // allow autoconversion from int/float to int/float
      // make config.cfg point to it
      config.cfg = &config_file_stuff;
      /* Get the Service Name. */
      if (config_lookup_string(config.cfg, "general.name", &str)) {
        raw_service_name = (char *)str;
      }

      /* Get the Daemonize setting. */
      config_set_lookup_bool(
          config.cfg, "sessioncontrol.daemonize_with_pid_file", &daemonisewith);

      /* Get the Just_Daemonize setting. */
      config_set_lookup_bool(config.cfg,
                             "sessioncontrol.daemonize_without_pid_file",
                             &daemonisewithout);

      /* Get the directory path for the pid file created when the program is
       * daemonised. */
      if (config_lookup_string(config.cfg, "sessioncontrol.daemon_pid_dir",
                               &str))
        config.piddir = (char *)str;

      /* Get the mdns_backend setting. */
      if (config_lookup_string(config.cfg, "general.mdns_backend", &str))
        config.mdns_name = (char *)str;

      /* Get the output_backend setting. */
      if (config_lookup_string(config.cfg, "general.output_backend", &str))
        config.output_name = (char *)str;

      /* Get the port setting. */
      if (config_lookup_int(config.cfg, "general.port", &value)) {
        if ((value < 0) || (value > 65535))

          die("Invalid port number  \"%sd\". It should be between 0 and 65535, "
              "default is 7000",
              value);
        else
          config.port = value;
      }

      /* Get the udp port base setting. */
      if (config_lookup_int(config.cfg, "general.udp_port_base", &value)) {
        if ((value < 0) || (value > 65535))
          die("Invalid port number  \"%sd\". It should be between 0 and 65535, "
              "default is 6001",
              value);
        else
          config.udp_port_base = value;
      }

      /* Get the udp port range setting. This is number of ports that will be
       * tried for free ports ,
       * starting at the port base. Only three ports are needed. */
      if (config_lookup_int(config.cfg, "general.udp_port_range", &value)) {
        if ((value < 3) || (value > 65535))
          die("Invalid port range  \"%sd\". It should be between 3 and 65535, "
              "default is 10",
              value);
        else
          config.udp_port_range = value;
      }

      /* Get the password setting. */
      if (config_lookup_string(config.cfg, "general.password", &str))
        config.password = (char *)str;

      /* Get the statistics setting. */
      if (config_set_lookup_bool(config.cfg, "general.statistics",
                                 &(config.statistics_requested))) {
        warn("The \"general\" \"statistics\" setting is deprecated. Please use "
             "the \"diagnostics\" "
             "\"statistics\" setting instead.");
      }

      /* The old drift tolerance setting. */
      if (config_lookup_int(config.cfg, "general.drift", &value)) {
        inform("The drift setting is deprecated. Use "
               "drift_tolerance_in_seconds instead");
        config.tolerance = 1.0 * value / 44100;
      }

      /* The old resync setting. */
      if (config_lookup_int(config.cfg, "general.resync_threshold", &value)) {
        inform("The resync_threshold setting is deprecated. Use "
               "resync_threshold_in_seconds instead");
        config.resyncthreshold = 1.0 * value / 44100;
      }

      /* Get the drift tolerance setting. */
      if (config_lookup_float(config.cfg, "general.drift_tolerance_in_seconds",
                              &dvalue))
        config.tolerance = dvalue;

      /* Get the resync setting. */
      if (config_lookup_float(config.cfg, "general.resync_threshold_in_seconds",
                              &dvalue))
        config.resyncthreshold = dvalue;

      /* Get the verbosity setting. */
      if (config_lookup_int(config.cfg, "general.log_verbosity", &value)) {
        warn("The \"general\" \"log_verbosity\" setting is deprecated. Please "
             "use the "
             "\"diagnostics\" \"log_verbosity\" setting instead.");
        if ((value >= 0) && (value <= 3))
          debuglev = value;
        else
          die("Invalid log verbosity setting option choice \"%d\". It should "
              "be between 0 and 3, "
              "inclusive.",
              value);
      }

      /* Get the verbosity setting. */
      if (config_lookup_int(config.cfg, "diagnostics.log_verbosity", &value)) {
        if ((value >= 0) && (value <= 3))
          debuglev = value;
        else
          die("Invalid diagnostics log_verbosity setting option choice \"%d\". "
              "It should be "
              "between 0 and 3, "
              "inclusive.",
              value);
      }

      /* Get the config.debugger_show_file_and_line in debug messages setting.
       */
      if (config_lookup_string(config.cfg, "diagnostics.log_show_file_and_line",
                               &str)) {
        if (strcasecmp(str, "no") == 0)
          config.debugger_show_file_and_line = 0;
        else if (strcasecmp(str, "yes") == 0)
          config.debugger_show_file_and_line = 1;
        else
          die("Invalid diagnostics log_show_file_and_line option choice "
              "\"%s\". It should be "
              "\"yes\" or \"no\"",
              str);
      }

      /* Get the show elapsed time in debug messages setting. */
      if (config_lookup_string(
              config.cfg, "diagnostics.log_show_time_since_startup", &str)) {
        if (strcasecmp(str, "no") == 0)
          config.debugger_show_elapsed_time = 0;
        else if (strcasecmp(str, "yes") == 0)
          config.debugger_show_elapsed_time = 1;
        else
          die("Invalid diagnostics log_show_time_since_startup option choice "
              "\"%s\". It should be "
              "\"yes\" or \"no\"",
              str);
      }

      /* Get the show relative time in debug messages setting. */
      if (config_lookup_string(config.cfg,
                               "diagnostics.log_show_time_since_last_message",
                               &str)) {
        if (strcasecmp(str, "no") == 0)
          config.debugger_show_relative_time = 0;
        else if (strcasecmp(str, "yes") == 0)
          config.debugger_show_relative_time = 1;
        else
          die("Invalid diagnostics log_show_time_since_last_message option "
              "choice \"%s\". It "
              "should be \"yes\" or \"no\"",
              str);
      }

      /* Get the statistics setting. */
      if (config_lookup_string(config.cfg, "diagnostics.statistics", &str)) {
        if (strcasecmp(str, "no") == 0)
          config.statistics_requested = 0;
        else if (strcasecmp(str, "yes") == 0)
          config.statistics_requested = 1;
        else
          die("Invalid diagnostics statistics option choice \"%s\". It should "
              "be \"yes\" or "
              "\"no\"",
              str);
      }

      /* Get the disable_resend_requests setting. */
      if (config_lookup_string(config.cfg,
                               "diagnostics.disable_resend_requests", &str)) {
        config.disable_resend_requests =
            0; // this is for legacy -- only set by -t 0
        if (strcasecmp(str, "no") == 0)
          config.disable_resend_requests = 0;
        else if (strcasecmp(str, "yes") == 0)
          config.disable_resend_requests = 1;
        else
          die("Invalid diagnostic disable_resend_requests option choice "
              "\"%s\". It should be "
              "\"yes\" "
              "or \"no\"",
              str);
      }

      /* Get the diagnostics output default. */
      if (config_lookup_string(config.cfg, "diagnostics.log_output_to", &str)) {
        if (strcasecmp(str, "syslog") == 0)
          log_to_syslog();
        else if (strcasecmp(str, "stdout") == 0) {
          log_to_stdout();
        } else if (strcasecmp(str, "stderr") == 0) {
          log_to_stderr();
        }
      }
      /* Get the ignore_volume_control setting. */
      if (config_lookup_string(config.cfg, "general.ignore_volume_control",
                               &str)) {
        if (strcasecmp(str, "no") == 0)
          config.ignore_volume_control = 0;
        else if (strcasecmp(str, "yes") == 0)
          config.ignore_volume_control = 1;
        else
          die("Invalid ignore_volume_control option choice \"%s\". It should "
              "be \"yes\" or \"no\"",
              str);
      }

      /* Get the optional volume_max_db setting. */
      if (config_lookup_float(config.cfg, "general.volume_max_db", &dvalue)) {
        // debug(1, "Max volume setting of %f dB", dvalue);
        config.volume_max_db = dvalue;
        config.volume_max_db_set = 1;
      }

      /* Get the playback_mode setting */
      if (config_lookup_string(config.cfg, "general.playback_mode", &str)) {
        if (strcasecmp(str, "stereo") == 0)
          config.playback_mode = ST_stereo;
        else if (strcasecmp(str, "mono") == 0)
          config.playback_mode = ST_mono;
        else if (strcasecmp(str, "reverse stereo") == 0)
          config.playback_mode = ST_reverse_stereo;
        else if (strcasecmp(str, "both left") == 0)
          config.playback_mode = ST_left_only;
        else if (strcasecmp(str, "both right") == 0)
          config.playback_mode = ST_right_only;
        else
          die("Invalid playback_mode choice \"%s\". It should be \"stereo\" "
              "(default), \"mono\", "
              "\"reverse stereo\", \"both left\", \"both right\"",
              str);
      }

      /* Get the volume control profile setting -- "standard" or "flat" */
      if (config_lookup_string(config.cfg, "general.volume_control_profile",
                               &str)) {
        if (strcasecmp(str, "standard") == 0)
          config.volume_control_profile = VCP_standard;
        else if (strcasecmp(str, "flat") == 0)
          config.volume_control_profile = VCP_flat;
        else
          die("Invalid volume_control_profile choice \"%s\". It should be "
              "\"standard\" (default) "
              "or \"flat\"",
              str);
      }

      config_set_lookup_bool(
          config.cfg, "general.volume_control_combined_hardware_priority",
          &config.volume_range_hw_priority);

      /* Get the interface to listen on, if specified Default is all interfaces
       */
      /* we keep the interface name and the index */

      if (config_lookup_string(config.cfg, "general.interface", &str))
        config.interface = strdup(str);

      if (config_lookup_string(config.cfg, "general.interface", &str)) {

        config.interface_index = if_nametoindex(config.interface);

        if (config.interface_index == 0) {
          inform("The mdns service interface \"%s\" was not found, so the "
                 "setting has been ignored.",
                 config.interface);
          free(config.interface);
          config.interface = NULL;
        }
      }

      /* Get the volume range, in dB, that should be used If not set, it means
       * you just use the range set by the mixer. */
      if (config_lookup_int(config.cfg, "general.volume_range_db", &value)) {
        if ((value < 30) || (value > 150))
          die("Invalid volume range  %d dB. It should be between 30 and 150 "
              "dB. Zero means use "
              "the mixer's native range. The setting reamins at %d.",
              value, config.volume_range_db);
        else
          config.volume_range_db = value;
      }

      /* Get the resend control settings. */
      if (config_lookup_float(
              config.cfg, "general.resend_control_first_check_time", &dvalue)) {
        if ((dvalue >= 0.0) && (dvalue <= 3.0))
          config.resend_control_first_check_time = dvalue;
        else
          warn("Invalid general resend_control_first_check_time setting "
               "\"%f\". It should "
               "be "
               "between 0.0 and 3.0, "
               "inclusive. The setting remains at %f seconds.",
               dvalue, config.resend_control_first_check_time);
      }

      if (config_lookup_float(config.cfg,
                              "general.resend_control_check_interval_time",
                              &dvalue)) {
        if ((dvalue >= 0.0) && (dvalue <= 3.0))
          config.resend_control_check_interval_time = dvalue;
        else
          warn("Invalid general resend_control_check_interval_time setting "
               "\"%f\". It should "
               "be "
               "between 0.0 and 3.0, "
               "inclusive. The setting remains at %f seconds.",
               dvalue, config.resend_control_check_interval_time);
      }

      if (config_lookup_float(
              config.cfg, "general.resend_control_last_check_time", &dvalue)) {
        if ((dvalue >= 0.0) && (dvalue <= 3.0))
          config.resend_control_last_check_time = dvalue;
        else
          warn("Invalid general resend_control_last_check_time setting \"%f\". "
               "It should "
               "be "
               "between 0.0 and 3.0, "
               "inclusive. The setting remains at %f seconds.",
               dvalue, config.resend_control_last_check_time);
      }

      /* Get the default latency. Deprecated! */

      if (config_lookup_float(config.cfg, "sessioncontrol.active_state_timeout",
                              &dvalue)) {
        if (dvalue < 0.0)
          warn("Invalid value \"%f\" for \"active_state_timeout\". It must be "
               "positive. "
               "The default of %f will be used instead.",
               dvalue, config.active_state_timeout);
        else
          config.active_state_timeout = dvalue;
      }

      if (config_lookup_string(
              config.cfg, "sessioncontrol.allow_session_interruption", &str)) {
        config.dont_check_timeout = 0; // this is for legacy -- only set by -t 0
        if (strcasecmp(str, "no") == 0)
          config.allow_session_interruption = 0;
        else if (strcasecmp(str, "yes") == 0)
          config.allow_session_interruption = 1;
        else
          die("Invalid \"allow_interruption\" option choice \"%s\". It should "
              "be "
              "\"yes\" "
              "or \"no\"",
              str);
      }

      if (config_lookup_int(config.cfg, "sessioncontrol.session_timeout",
                            &value)) {
        config.timeout = value;
        config.dont_check_timeout = 0; // this is for legacy -- only set by -t 0
      }

      if (config_lookup_string(config.cfg, "dsp.loudness", &str)) {
        if (strcasecmp(str, "no") == 0)
          config.loudness = 0;
        else if (strcasecmp(str, "yes") == 0)
          config.loudness = 1;
        else
          die("Invalid dsp.loudness \"%s\". It should be \"yes\" or \"no\"",
              str);
      }

      if (config_lookup_float(config.cfg, "dsp.loudness_reference_volume_db",
                              &dvalue)) {
        config.loudness_reference_volume_db = dvalue;
        if (dvalue > 0 || dvalue < -100)
          die("Invalid value \"%f\" for dsp.loudness_reference_volume_db. It "
              "should be between "
              "-100 and 0",
              dvalue);
      }

      if (config.loudness == 1 &&
          config_lookup_string(config.cfg, "alsa.mixer_control_name", &str))
        die("Loudness activated but hardware volume is active. You must remove "
            "\"alsa.mixer_control_name\" to use the loudness filter.");
    } else {
      if (config_error_type(&config_file_stuff) == CONFIG_ERR_FILE_IO)
        debug(2, "Error reading configuration file \"%s\": \"%s\".",
              config_error_file(&config_file_stuff),
              config_error_text(&config_file_stuff));
      else {
        die("Line %d of the configuration file \"%s\":\n%s",
            config_error_line(&config_file_stuff),
            config_error_file(&config_file_stuff),
            config_error_text(&config_file_stuff));
      }
    }

    long long aid;

    // replace the airplay_device_id with this, if provided
    if (config_lookup_int64(config.cfg, "general.airplay_device_id", &aid)) {
      temporary_airplay_id = aid;
    }

    // add the airplay_device_id_offset if provided
    if (config_lookup_int64(config.cfg, "general.airplay_device_id_offset",
                            &aid)) {
      temporary_airplay_id += aid;
    }
  }

  // now, do the command line options again, but this time do them fully -- it's
  // a unix convention that command line arguments have precedence over
  // configuration file settings.

  optind = argc;
  for (j = 0; j < argc; j++)
    if (strcmp(argv[j], "--") == 0)
      optind = j;

  optCon = poptGetContext(NULL, optind, (const char **)argv, optionsTable, 0);
  if (optCon == NULL)
    die("Can not get a popt context.");
  poptSetOtherOptionHelp(optCon, "[OPTIONS]* ");

  /* Now do options processing, get portname */
  int tdebuglev = 0;
  while ((c = poptGetNextOpt(optCon)) >= 0) {
    switch (c) {
    case 'v':
      tdebuglev++;
      break;
    case 't':
      if (config.timeout == 0) {
        config.dont_check_timeout = 1;
        config.allow_session_interruption = 1;
      } else {
        config.dont_check_timeout = 0;
        config.allow_session_interruption = 0;
      }
      break;
    }
  }
  if (c < -1) {
    die("%s: %s", poptBadOption(optCon, POPT_BADOPTION_NOALIAS),
        poptStrerror(c));
  }

  poptFreeContext(optCon);

  // here, we are finally finished reading the options

  // finish the Airplay 2 options

  char shared_memory_interface_name[256] = "";
  snprintf(shared_memory_interface_name, sizeof(shared_memory_interface_name),
           "/%s-%" PRIx64 "", config.appName, temporary_airplay_id);
  // debug(1, "smi name: \"%s\"", shared_memory_interface_name);

  config.nqptp_shared_memory_interface_name =
      strdup(shared_memory_interface_name);

  char apids[6 * 2 + 5 + 1]; // six pairs of digits, 5 colons and a NUL
  apids[6 * 2 + 5] = 0;      // NUL termination
  int i;
  char hexchar[] = "0123456789abcdef";
  for (i = 5; i >= 0; i--) {
    apids[i * 3 + 1] = hexchar[temporary_airplay_id & 0xF];
    temporary_airplay_id = temporary_airplay_id >> 4;
    apids[i * 3] = hexchar[temporary_airplay_id & 0xF];
    temporary_airplay_id = temporary_airplay_id >> 4;
    if (i != 0)
      apids[i * 3 - 1] = ':';
  }

  config.airplay_device_id = strdup(apids);

  if ((daemonisewith) && (daemonisewithout))
    die("Select either daemonize_with_pid_file or daemonize_without_pid_file "
        "-- you have selected "
        "both!");
  if ((daemonisewith) || (daemonisewithout)) {
    config.daemonise = 1;
    if (daemonisewith)
      config.daemonise_store_pid = 1;
  };

  /* if the regtype2 hasn't been set, do it now */
  if (config.regtype2 == NULL)
    config.regtype2 = strdup("_airplay._tcp");

  if (tdebuglev != 0)
    debuglev = tdebuglev;

  // now, do the substitutions in the service name
  char hostname[100];
  gethostname(hostname, 100);

  char *i0;
  if (raw_service_name == NULL)
    i0 = strdup(
        "%H"); // this is the default it the Service Name wasn't specified
  else
    i0 = strdup(raw_service_name);

  // here, do the substitutions for %h, %H, %v and %V
  char *i1 = str_replace(i0, "%h", hostname);
  if ((hostname[0] >= 'a') && (hostname[0] <= 'z'))
    hostname[0] =
        hostname[0] -
        0x20; // convert a lowercase first letter into a capital letter
  char *i2 = str_replace(i1, "%H", hostname);
  char *i3 = str_replace(i2, "%v", PACKAGE_VERSION);
  char *vs = get_version_string();
  config.service_name = str_replace(i3, "%V", vs); // service name complete
  free(i0);
  free(i1);
  free(i2);
  free(i3);
  free(vs);

// now, check and calculate the pid directory
#ifdef DEFINED_CUSTOM_PID_DIR
  char *use_this_pid_dir = PIDDIR;
#else
  char temp_pid_dir[4096];
  strcpy(temp_pid_dir, "/var/run/");
  strcat(temp_pid_dir, config.appName);
  debug(1, "default pid filename is \"%s\".", temp_pid_dir);
  char *use_this_pid_dir = temp_pid_dir;
#endif
  // debug(1,"config.piddir \"%s\".",config.piddir);
  if (config.piddir)
    use_this_pid_dir = config.piddir;
  if (use_this_pid_dir)
    config.computed_piddir = strdup(use_this_pid_dir);
  return optind + 1;
}

char pid_file_path_string[4096] = "\0";

const char *pid_file_proc(void) {
  snprintf(pid_file_path_string, sizeof(pid_file_path_string), "%s/%s.pid",
           config.computed_piddir,
           daemon_pid_file_ident ? daemon_pid_file_ident : "unknown");
  // debug(1,"pid_file_path_string \"%s\".",pid_file_path_string);
  return pid_file_path_string;
}

void exit_rtsp_listener() {
  pthread_cancel(rtsp_listener_thread);
  pthread_join(rtsp_listener_thread, NULL); // not sure you need this
}

void exit_function() {

  if (emergency_exit == 0) {
    // the following is to ensure that if libdaemon has been included
    // that most of this code will be skipped when the parent process is exiting
    // exec

    if ((this_is_the_daemon_process) ||
        (config.daemonise ==
         0)) { // if this is the daemon process that is exiting or it's not
      // actually daemonised at all

      debug(2, "exit function called...");
      /*
      Actually, there is no terminate_mqtt() function.
      #ifdef CONFIG_MQTT
              if (config.mqtt_enabled) {
                      terminate_mqtt();
              }
      #endif
      */

      if (config.regtype2)
        free(config.regtype2);
      if (config.nqptp_shared_memory_interface_name)
        free(config.nqptp_shared_memory_interface_name);
      if (config.airplay_device_id)
        free(config.airplay_device_id);
      if (config.airplay_pin)
        free(config.airplay_pin);
      if (config.airplay_pi)
        free(config.airplay_pi);
      ptp_shm_interface_close(); // close it if it's open

      if (this_is_the_daemon_process) {
        daemon_retval_send(0);
        daemon_pid_file_remove();
        daemon_signal_done();
        if (config.computed_piddir)
          free(config.computed_piddir);
      }
    }

    if (config.cfg)
      config_destroy(config.cfg);
    if (config.appName)
      free(config.appName);

    // probably should be freeing malloc'ed memory here, including
    // strdup-created strings...

    if (this_is_the_daemon_process) { // this is the daemon that is exiting
      debug(1, "libdaemon daemon exit");
    } else {
      if (config.daemonise)
        debug(1, "libdaemon parent exit");
      else
        debug(1, "exit_function libdaemon exit");
    }
  } else {
    debug(1, "emergency exit");
  }
}

// for removing zombie script processes
// see:
// http://www.microhowto.info/howto/reap_zombie_processes_using_a_sigchld_handler.html
// used with thanks.

void handle_sigchld(__attribute__((unused)) int sig) {
  int saved_errno = errno;
  while (waitpid((pid_t)(-1), 0, WNOHANG) > 0) {
  }
  errno = saved_errno;
}

// for clean exits
void intHandler(__attribute__((unused)) int k) {
  debug(2, "exit on SIGINT");
  exit(EXIT_SUCCESS);
}

void termHandler(__attribute__((unused)) int k) {
  debug(2, "exit on SIGTERM");
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  /* Check if we are called with -V or --version parameter */
  if (argc >= 2 &&
      ((strcmp(argv[1], "-V") == 0) || (strcmp(argv[1], "--version") == 0))) {
    print_version();
    exit(EXIT_SUCCESS);
  }

  /* Check if we are called with -h or --help parameter */
  if (argc >= 2 &&
      ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
    usage(argv[0]);
    exit(EXIT_SUCCESS);
  }

  log_to_syslog();

  pid = getpid();
  config.log_fd = -1;
  conns = NULL; // no connections active
  memset((void *)&main_thread_id, 0, sizeof(main_thread_id));
  memset(&config, 0, sizeof(config)); // also clears all strings, BTW
  ns_time_at_startup = get_absolute_time_in_ns();
  ns_time_at_last_debug_message = ns_time_at_startup;
  // this is a bit weird, but necessary -- basename() may modify the argument
  // passed in
  char *basec = strdup(argv[0]);
  char *bname = basename(basec);
  config.appName = strdup(bname);
  if (config.appName == NULL)
    die("can not allocate memory for the app name!");
  free(basec);

  //  debug(1,"startup");

  daemon_set_verbosity(LOG_DEBUG);

  emergency_exit = 0; // what to do or skip in the exit_function
  atexit(exit_function);

  // set defaults

  // get a device id -- the first non-local MAC address
  get_device_id((uint8_t *)&config.hw_addr, 6);

  // get the endianness
  union {
    uint32_t u32;
    uint8_t arr[4];
  } xn;

  xn.arr[0] = 0x44; /* Lowest-address byte */
  xn.arr[1] = 0x33;
  xn.arr[2] = 0x22;
  xn.arr[3] = 0x11; /* Highest-address byte */

  if (xn.u32 == 0x11223344)
    config.endianness = SS_LITTLE_ENDIAN;
  else if (xn.u32 == 0x33441122)
    config.endianness = SS_PDP_ENDIAN;
  else if (xn.u32 == 0x44332211)
    config.endianness = SS_BIG_ENDIAN;
  else
    die("Can not recognise the endianness of the processor.");

  // set non-zero / non-NULL default values here
  // but note that audio back ends also have a chance to set defaults

  // get the first output backend in the list and make it the default
  audio_output *first_backend = audio_get_output(NULL);
  if (first_backend == NULL) {
    die("No audio backend found! Check your build of Shairport Sync.");
  } else {
    strncpy(first_backend_name, first_backend->name,
            sizeof(first_backend_name) - 1);
    config.output_name = first_backend_name;
  }

  strcpy(configuration_file_path, SYSCONFDIR);
  // strcat(configuration_file_path, "/shairport-sync"); // thinking about
  // adding a special shairport-sync directory
  strcat(configuration_file_path, "/");
  strcat(configuration_file_path, config.appName);
  strcat(configuration_file_path, ".conf");
  config.configfile = configuration_file_path;

  // config.statistics_requested = 0; // don't print stats in the log

  config.debugger_show_file_and_line =
      1; // by default, log the file and line of the originating message
  config.debugger_show_relative_time =
      1; // by default, log the  time back to the previous debug message
  config.resyncthreshold = 0.05; // 50 ms
  config.timeout = 120;     // this number of seconds to wait for [more] audio
                            // before switching to idle.
  config.tolerance = 0.002; // this number of seconds of timing error before
                            // attempting to correct it.
  config.buffer_start_fill = 220;

  config.timeout = 0; // disable watchdog
  config.port = 7000;

  // char hostname[100];
  // gethostname(hostname, 100);
  // config.service_name = malloc(20 + 100);
  // snprintf(config.service_name, 20 + 100, "Shairport Sync on %s", hostname);
  set_requested_connection_state_to_output(
      1); // we expect to be able to connect to the output device
  config.audio_backend_buffer_desired_length = 0.15; // seconds
  config.udp_port_base = 6001;
  config.udp_port_range = 10;
  config.output_format = SPS_FORMAT_S16_LE; // default
  config.output_format_auto_requested = 1;  // default auto select format
  config.output_rate = 44100;               // default
  config.output_rate_auto_requested = 1;    // default auto select format
  config.decoders_supported =
      1 << decoder_hammerton; // David Hammerton's decoder supported by default

  // initialise random number generator

  r64init(0);

  /* Reset signal handlers */
  if (daemon_reset_sigs(-1) < 0) {
    daemon_log(LOG_ERR, "Failed to reset all signal handlers: %s",
               strerror(errno));
    return 1;
  }

  /* Unblock signals */
  if (daemon_unblock_sigs(-1) < 0) {
    daemon_log(LOG_ERR, "Failed to unblock all signals: %s", strerror(errno));
    return 1;
  }

  /* Set identification string for the daemon for both syslog and PID file */
  daemon_pid_file_ident = daemon_log_ident = daemon_ident_from_argv0(argv[0]);

  daemon_pid_file_proc = pid_file_proc;

  // parse arguments into config -- needed to locate pid_dir
  int audio_arg = parse_options(argc, argv);

  // mDNS supports maximum of 63-character names (we append 13).
  if (strlen(config.service_name) > 50) {
    warn("Supplied name too long (max 50 characters)");
    config.service_name[50] = '\0'; // truncate it and carry on...
  }

  /* Check if we are called with -k or --kill option */
  if (killOption != 0) {

    int ret;

    /* Kill daemon with SIGTERM */
    /* Check if the new function daemon_pid_file_kill_wait() is available, if it
     * is, use it. */
    if ((ret = daemon_pid_file_kill_wait(SIGTERM, 5)) < 0) {
      if (errno == ENOENT)
        daemon_log(LOG_WARNING, "Failed to kill %s daemon: PID file not found.",
                   config.appName);
      else
        daemon_log(LOG_WARNING, "Failed to kill %s daemon: \"%s\", errno %u.",
                   config.appName, strerror(errno), errno);
    } else {
      // debug(1,"Successfully killed the %s daemon.", config.appName);
      if (daemon_pid_file_remove() == 0)
        debug(2, "killed the %s daemon.", config.appName);
      else
        daemon_log(LOG_WARNING,
                   "killed the %s daemon, but cannot remove old PID file: "
                   "\"%s\", errno %u.",
                   config.appName, strerror(errno), errno);
    }
    return ret < 0 ? 1 : 0;
  }

  /* If we are going to daemonise, check that the daemon is not running
   * already.*/
  if ((config.daemonise) && ((pid = daemon_pid_file_is_running()) >= 0)) {
    daemon_log(LOG_ERR, "The %s daemon is already running as PID %u",
               config.appName, pid);
    return 1;
  }

  /* here, daemonise with libdaemon */

  if (config.daemonise) {
    /* Prepare for return value passing from the initialization procedure of the
     * daemon process */
    if (daemon_retval_init() < 0) {
      daemon_log(LOG_ERR, "Failed to create pipe.");
      return 1;
    }

    /* Do the fork */
    if ((pid = daemon_fork()) < 0) {

      /* Exit on error */
      daemon_retval_done();
      return 1;
    } else if (pid) { /* The parent */
      int ret;

      /* Wait for 20 seconds for the return value passed from the daemon process
       */
      if ((ret = daemon_retval_wait(20)) < 0) {
        daemon_log(LOG_ERR,
                   "Could not receive return value from daemon process: %s",
                   strerror(errno));
        return 255;
      }

      switch (ret) {
      case 0:
        break;
      case 1:
        daemon_log(LOG_ERR,
                   "the %s daemon failed to launch: could not close open file "
                   "descriptors after forking.",
                   config.appName);
        break;
      case 2:
        daemon_log(LOG_ERR,
                   "the %s daemon failed to launch: could not create PID file.",
                   config.appName);
        break;
      case 3:
        daemon_log(LOG_ERR,
                   "the %s daemon failed to launch: could not create or access "
                   "PID directory.",
                   config.appName);
        break;
      default:
        daemon_log(LOG_ERR, "the %s daemon failed to launch, error %i.",
                   config.appName, ret);
      }
      return ret;
    } else { /* pid == 0 means we are the daemon */

      this_is_the_daemon_process = 1; //

      /* Close FDs */
      if (daemon_close_all(-1) < 0) {
        daemon_log(LOG_ERR, "Failed to close all file descriptors: %s",
                   strerror(errno));
        /* Send the error condition to the parent process */
        daemon_retval_send(1);

        daemon_signal_done();
        return 0;
      }

      /* Create the PID file if required */
      if (config.daemonise_store_pid) {
        /* Create the PID directory if required -- we don't really care about
         * the result */
        printf("PID directory is \"%s\".", config.computed_piddir);
        int result = mkpath(config.computed_piddir, 0700);
        if ((result != 0) && (result != -EEXIST)) {
          // error creating or accessing the PID file directory
          daemon_retval_send(3);

          daemon_signal_done();
          return 0;
        }

        if (daemon_pid_file_create() < 0) {
          daemon_log(LOG_ERR, "Could not create PID file (%s).",
                     strerror(errno));

          daemon_retval_send(2);
          daemon_signal_done();
          return 0;
        }
      }

      /* Send OK to parent process */
      daemon_retval_send(0);
    }
    /* end libdaemon stuff */
  }

  uint64_t apf = config.airplay_features;
  uint64_t apfh = config.airplay_features;
  apfh = apfh >> 32;
  uint32_t apf32 = apf;
  uint32_t apfh32 = apfh;
  debug(1,
        "Started in Airplay 2 mode with features 0x%" PRIx32 ",0x%" PRIx32
        " on device \"%s\"!",
        apf32, apfh32, config.airplay_device_id);

  // control-c (SIGINT) cleanly
  struct sigaction act;
  memset(&act, 0, sizeof(struct sigaction));
  act.sa_handler = intHandler;
  sigaction(SIGINT, &act, NULL);

  // terminate (SIGTERM)
  struct sigaction act2;
  memset(&act2, 0, sizeof(struct sigaction));
  act2.sa_handler = termHandler;
  sigaction(SIGTERM, &act2, NULL);

  // stop a pipe signal from killing the program
  signal(SIGPIPE, SIG_IGN);

  // install a zombie process reaper
  // see:
  // http://www.microhowto.info/howto/reap_zombie_processes_using_a_sigchld_handler.html
  struct sigaction sa;
  sa.sa_handler = &handle_sigchld;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  if (sigaction(SIGCHLD, &sa, 0) == -1) {
    perror(0);
    exit(1);
  }

  main_thread_id = pthread_self();
  if (!main_thread_id)
    debug(1, "Main thread is set up to be NULL!");

  // make sure the program can create files that group and world can read
  umask(S_IWGRP | S_IWOTH);

  /* print out version */

  char *version_dbs = get_version_string();
  if (version_dbs) {
    debug(1, "software version: \"%s\"", version_dbs);
    free(version_dbs);
  } else {
    debug(1, "can't print the version information!");
  }

  debug(1, "log verbosity is %d.", debuglev);

  config.output = audio_get_output(config.output_name);
  if (!config.output) {
    die("Invalid audio backend \"%s\" selected!",
        config.output_name == NULL ? "<unspecified>" : config.output_name);
  }
  config.output->init(argc - audio_arg, argv + audio_arg);

  // pthread_cleanup_push(main_cleanup_handler, NULL);

  // daemon_log(LOG_NOTICE, "startup");

  switch (config.endianness) {
  case SS_LITTLE_ENDIAN:
    debug(2, "The processor is running little-endian.");
    break;
  case SS_BIG_ENDIAN:
    debug(2, "The processor is running big-endian.");
    break;
  case SS_PDP_ENDIAN:
    debug(2, "The processor is running pdp-endian.");
    break;
  }

  if (sodium_init() < 0) {
    debug(1, "Can't initialise libsodium!");
  } else {
    debug(1, "libsodium initialised.");
  }

  // this code is based on
  // https://www.gnupg.org/documentation/manuals/gcrypt/Initializing-the-library.html

  /* Version check should be the very first call because it
    makes sure that important subsystems are initialized.
    #define NEED_LIBGCRYPT_VERSION to the minimum required version. */

#define NEED_LIBGCRYPT_VERSION "1.5.4"

  if (!gcry_check_version(NEED_LIBGCRYPT_VERSION)) {
    die("libgcrypt is too old (need %s, have %s).", NEED_LIBGCRYPT_VERSION,
        gcry_check_version(NULL));
  }

  /* Disable secure memory.  */
  gcry_control(GCRYCTL_DISABLE_SECMEM, 0);

  /* ... If required, other initialization goes here.  */

  /* Tell Libgcrypt that initialization has completed. */
  gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);

  /* Mess around with the latency options */
  // Basically, we expect the source to set the latency and add a fixed offset
  // of 11025 frames to it, which sounds right If this latency is outside the
  // max and min latensies that may be set by the source, clamp it to fit.

  /* Print out options */
  debug(1, "disable resend requests is %s.",
        config.disable_resend_requests ? "on" : "off");
  debug(1, "statistics_requester status is %d.", config.statistics_requested);

  debug(1, "daemon status is %d.", config.daemonise);
  debug(1, "daemon pid file path is \"%s\".", pid_file_proc());
  debug(1, "rtsp listening port is %d.", config.port);
  debug(1, "udp base port is %d.", config.udp_port_base);
  debug(1, "udp port range is %d.", config.udp_port_range);
  debug(1, "player name is \"%s\".", config.service_name);
  debug(1, "backend is \"%s\".", config.output_name);

  debug(1, "active_state_timeout is  %f seconds.", config.active_state_timeout);
  debug(1, "mdns backend \"%s\".", strnull(config.mdns_name));

  debug(1, "resync time is %f seconds.", config.resyncthreshold);
  debug(1, "allow a session to be interrupted: %d.",
        config.allow_session_interruption);
  debug(1, "busy timeout time is %d.", config.timeout);
  debug(1, "drift tolerance is %f seconds.", config.tolerance);
  debug(1, "password is \"%s\".", strnull(config.password));
  debug(1, "ignore_volume_control is %d.", config.ignore_volume_control);
  if (config.volume_max_db_set)
    debug(1, "volume_max_db is %d.", config.volume_max_db);
  else
    debug(1, "volume_max_db is not set");
  debug(1,
        "volume range in dB (zero means use the range specified by the mixer): "
        "%u.",
        config.volume_range_db);
  debug(1,
        "volume_range_combined_hardware_priority (1 means hardware mixer "
        "attenuation is used "
        "first) is %d.",
        config.volume_range_hw_priority);
  debug(1,
        "playback_mode is %d (0-stereo, 1-mono, 1-reverse_stereo, 2-both_left, "
        "3-both_right).",
        config.playback_mode);

  debug(1, "output_format automatic selection is %sabled.",
        config.output_format_auto_requested ? "en" : "dis");
  if (config.output_format_auto_requested == 0)
    debug(1, "output_format is \"%s\".",
          sps_format_description_string(config.output_format));
  debug(1, "output_rate automatic selection is %sabled.",
        config.output_rate_auto_requested ? "en" : "dis");
  if (config.output_rate_auto_requested == 0)
    debug(1, "output_rate is %d.", config.output_rate);
  debug(1, "audio backend desired buffer length is %f seconds.",
        config.audio_backend_buffer_desired_length);
  debug(
      1,
      "audio_backend_buffer_interpolation_threshold_in_seconds is %f seconds.",
      config.audio_backend_buffer_interpolation_threshold_in_seconds);
  debug(1, "audio backend latency offset is %f seconds.",
        config.audio_backend_latency_offset);
  if (config.audio_backend_silent_lead_in_time_auto == 1)
    debug(1, "audio backend silence lead-in time is \"auto\".");
  else
    debug(1, "audio backend silence lead-in time is %f seconds.",
          config.audio_backend_silent_lead_in_time);

  debug(1, "decoders_supported field is %d.", config.decoders_supported);

  debug(1, "alsa_use_hardware_mute is %d.", config.alsa_use_hardware_mute);
  if (config.interface)
    debug(1, "mdns service interface \"%s\" requested.", config.interface);
  else
    debug(1, "no special mdns service interface was requested.");
  char *realConfigPath = realpath(config.configfile, NULL);
  if (realConfigPath) {
    debug(1, "configuration file name \"%s\" resolves to \"%s\".",
          config.configfile, realConfigPath);
    free(realConfigPath);
  } else {
    debug(1, "configuration file name \"%s\" can not be resolved.",
          config.configfile);
  }

  ptp_send_control_message_string(
      "T"); // get nqptp to create the named shm interface
  int ptp_check_times = 0;
  const int ptp_wait_interval_us = 5000;
  // wait for up to two seconds for NQPTP to come online
  do {
    ptp_send_control_message_string(
        "T"); // get nqptp to create the named shm interface
    usleep(ptp_wait_interval_us);
    ptp_check_times++;
  } while ((ptp_shm_interface_open() != 0) &&
           (ptp_check_times < (2000000 / ptp_wait_interval_us)));

  if (ptp_shm_interface_open() != 0) {
    die("Can't access NQPTP! Is it installed and running?");
  } else {
    if (ptp_check_times == 1)
      debug(1, "NQPTP is online.");
    else
      debug(1, "NQPTP is online after %u microseconds.",
            ptp_check_times * ptp_wait_interval_us);
  }

  activity_monitor_start(); // not yet for AP2
  pthread_create(&rtsp_listener_thread, NULL, &rtsp_listen_loop, NULL);
  atexit(exit_rtsp_listener);
  pthread_join(rtsp_listener_thread, NULL);
  return 0;
}
