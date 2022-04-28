#ifdef __cplusplus
extern "C" {
#endif

#ifndef _COMMON_H
#define _COMMON_H

#include <libconfig.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "audio.h"
#include "config.h"
#include "definitions.h"
#include "mdns.h"

// struct sockaddr_in6 is bigger than struct sockaddr. derp
#ifdef AF_INET6
#define SOCKADDR struct sockaddr_storage
#define SAFAMILY ss_family
#else
#define SOCKADDR struct sockaddr
#define SAFAMILY sa_family
#endif

#define sps_extra_code_output_stalled 32768
#define sps_extra_code_output_state_cannot_make_ready 32769

// yeah/no/auto
typedef enum { YNA_AUTO = -1, YNA_NO = 0, YNA_YES = 1 } yna_type;

// yeah/no/dont-care
typedef enum { YNDK_DONT_KNOW = -1, YNDK_NO = 0, YNDK_YES = 1 } yndk_type;

typedef enum {
  SS_LITTLE_ENDIAN = 0,
  SS_PDP_ENDIAN,
  SS_BIG_ENDIAN,
} endian_type;

typedef enum {
  ST_basic = 0, // straight deletion or insertion of a frame in a 352-frame packet
  ST_soxr,      // use libsoxr to make a 352 frame packet one frame longer or shorter
  ST_auto,      // use soxr if compiled for it and if the soxr_index is low enough
} stuffing_type;

typedef enum {
  ST_stereo = 0,
  ST_mono,
  ST_reverse_stereo,
  ST_left_only,
  ST_right_only,
} playback_mode_type;

typedef enum {
  VCP_standard = 0,
  VCP_flat,
} volume_control_profile_type;

typedef enum {
  decoder_hammerton = 0,
  decoder_apple_alac,
} decoders_supported_type;

typedef enum {
  disable_standby_off = 0,
  disable_standby_auto,
  disable_standby_always
} disable_standby_mode_type;

// the following enum is for the formats recognised -- currently only S16LE is
// recognised for input, so these are output only for the present

typedef enum {
  SPS_FORMAT_UNKNOWN = 0,
  SPS_FORMAT_S8,
  SPS_FORMAT_U8,
  SPS_FORMAT_S16,
  SPS_FORMAT_S16_LE,
  SPS_FORMAT_S16_BE,
  SPS_FORMAT_S24,
  SPS_FORMAT_S24_LE,
  SPS_FORMAT_S24_BE,
  SPS_FORMAT_S24_3LE,
  SPS_FORMAT_S24_3BE,
  SPS_FORMAT_S32,
  SPS_FORMAT_S32_LE,
  SPS_FORMAT_S32_BE,
  SPS_FORMAT_AUTO,
  SPS_FORMAT_INVALID,
} sps_format_t;

const char *sps_format_description_string(sps_format_t format);

typedef struct {
  // wait this long before asking for a missing packet to be resent
  double resend_control_first_check_time;

  // wait this long between making requests
  double resend_control_check_interval_time;

  // if the packet is missing this close to the time of use, give up
  double resend_control_last_check_time;
  pthread_mutex_t lock;
  config_t *cfg;
  int endianness;
  double airplay_volume; // stored here for reloading when necessary

  // normally the app is called shairport-syn, but it may be symlinked
  char *appName;
  char *password;

  // the name for the shairport service, e.g. "Shairport
  // Sync Version %v running on host %h"
  char *service_name;

  // only needs 6 but 8 is handy when converting this to a number
  uint8_t hw_addr[8];
  int port;
  int udp_port_base;
  int udp_port_range;
  int ignore_volume_control;

  // set to 1 if a maximum volume db has been set
  int volume_max_db_set;
  int volume_max_db;

  // if it get's out of whack my more than this number
  // of seconds, resync. Zero means never resync.
  double resyncthreshold;
  int allow_session_interruption;

  // while in play mode, exit if no packets of audio come in for
  // more than this number of seconds . Zero means never exit.
  int timeout;

  // this is used to maintain backward compatibility
  // with the old -t option behaviour; only set by -t 0,
  // cleared by everything else
  int dont_check_timeout;
  char *output_name;
  audio_output *output;
  char *mdns_name;
  mdns_backend *mdns;

  int buffer_start_fill;

  // add this to all automatic latencies supplied
  // to get the actual total latency the total
  // latency will be limited to the min and
  // max-latency values, if supplied
  uint32_t fixedLatencyOffset;

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
  playback_mode_type playback_mode;
  double tolerance; // allow this much drift before attempting to correct it

  int decoders_supported;

  // char *logfile;
  // char *errfile;
  char *configfile;

  // The regtype is the service type followed by the protocol,
  // separated by a dot, by default “_raop._tcp.” for AirPlay 2.
  char *regtype2;

  // a string containg the interface name, or NULL if nothing specified
  char *interface;

  // only valid if the interface string is non-NULL
  int interface_index;

  // this will be the length in seconds of the audio backend buffer -- the
  // DAC buffer for ALSA
  double audio_backend_buffer_desired_length;

  // below this, soxr interpolation will not occur -- it'll be
  // basic interpolation instead.
  double audio_backend_buffer_interpolation_threshold_in_seconds;

  // below this, silence will be added to the output buffer
  double disable_standby_mode_silence_threshold;

  // check the threshold this often
  double disable_standby_mode_silence_scan_interval;

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

  // when effective latency is
  // calculated, ensure this number of
  // buffers are unallocated
  uint32_t minimum_free_buffer_headroom;
  double active_state_timeout;  // the amount of time from when play ends to when
                                // the system leaves into the "active" mode.
  uint32_t volume_range_db;     // the range, in dB, from max dB to min dB. Zero
                                // means use the mixer's native range.
  int volume_range_hw_priority; // when extending the volume range by combining
                                // sw and hw attenuators, lowering the volume,
                                // use all the hw attenuation before using sw
                                // attenuation
  volume_control_profile_type volume_control_profile;

  int output_format_auto_requested; // true if the configuration requests auto
                                    // configuration
  sps_format_t output_format;
  int output_rate_auto_requested; // true if the configuration requests auto
                                  // configuration
  unsigned int output_rate;

  int loudness;
  float loudness_reference_volume_db;
  int alsa_use_hardware_mute;
  double alsa_maximum_stall_time;
  disable_standby_mode_type disable_standby_mode;
  volatile int keep_dac_busy;
  yna_type use_precision_timing; // defaults to no

  // set this to stop resend request being made for missing packets
  int disable_resend_requests;

  // a linked list of the clock gradients discovered for all
  // DACP IDs can't use IP numbers as they might be given to
  // different devices can't get hold of MAC addresses. can't
  // define the nvll linked list struct here
  void *gradients;

  uint64_t airplay_features;
  uint32_t airplay_statusflags;
  // for the Bonjour advertisement and the GETINFO PList
  char *airplay_device_id;

  // non-NULL, 4 char PIN, if required for pairing
  char *airplay_pin;

  // UUID in the Bonjour advertisement and the GETINFO Plist
  char *airplay_pi;

  // client name for nqptp service
  char *nqptp_shared_memory_interface_name;

} shairport_cfg;

// accessors to config for multi-thread access
double get_config_airplay_volume();
void set_config_airplay_volume(double v);

// read 4 characters from *p and do ntohl on them
uint32_t nctohl(const uint8_t *p);

// read 2 characters from *p and do ntohs on them
uint16_t nctohs(const uint8_t *p);

// read 8 characters from *p to a uint64_t
uint64_t nctoh64(const uint8_t *p);

void memory_barrier();

void log_to_stderr(); // call this to direct logging to stderr;
void log_to_stdout(); // call this to direct logging to stdout;
void log_to_syslog(); // call this to direct logging to the system log;

// true if Shairport Sync is supposed to be sending output to the output device,
// false otherwise

int get_requested_connection_state_to_output();

void set_requested_connection_state_to_output(int v);

// open it without blocking if it's not hooked up
int try_to_open_pipe_for_writing(const char *pathname);

/* from
 * http://coding.debuntu.org/c-implementing-str_replace-replace-all-occurrences-substring#comment-722
 */
char *str_replace(const char *string, const char *substr, const char *replacement);

// based on http://burtleburtle.net/bob/rand/smallprng.html

void r64init(uint64_t seed);
uint64_t r64u();
int64_t r64i();

// if you are breaking in to a session, you need to avoid the ports of the
// current session if you are law-abiding, then you can reuse the ports. so, you
// can reset the free UDP ports minder when you're legit, and leave it otherwise

// the downside of using different ports each time is that it might make the
// firewall rules a bit more complex, as they need to allow more than the
// minimum three ports. a range of 10 is suggested anyway

void resetFreeUDPPort();
uint16_t nextFreeUDPPort();

extern volatile int debuglev;

void _die(const char *filename, const int linenumber, const char *format, ...);
void _warn(const char *filename, const int linenumber, const char *format, ...);
void _inform(const char *filename, const int linenumber, const char *format, ...);
void _debug(const char *filename, const int linenumber, int level, const char *format, ...);

#define die(...) _die(__FILE__, __LINE__, __VA_ARGS__)
#define debug(...) _debug(__FILE__, __LINE__, __VA_ARGS__)
#define warn(...) _warn(__FILE__, __LINE__, __VA_ARGS__)
#define inform(...) _inform(__FILE__, __LINE__, __VA_ARGS__)

uint8_t *base64_dec(char *input, int *outlen);
char *base64_enc(uint8_t *input, int length);

#define RSA_MODE_AUTH (0)
#define RSA_MODE_KEY (1)

uint8_t *rsa_apply(uint8_t *input, int inlen, int *outlen, int mode);

// given a volume (0 to -30) and high and low attenuations in dB*100 (e.g. 0 to
// -6000 for 0 to -60 dB), return an attenuation depending on a linear
// interpolation along along the range
double flat_vol2attn(double vol, long max_db, long min_db);

// given a volume (0 to -30) and high and low attenuations in dB*100 (e.g. 0 to
// -6000 for 0 to -60 dB), return an attenuation depending on the transfer
// function
double vol2attn(double vol, long max_db, long min_db);

// return a time in nanoseconds

// Not defined for macOS
uint64_t get_realtime_in_ns(void);
uint64_t get_absolute_time_in_ns(void);  // monotonic_raw or monotonic
uint64_t get_monotonic_time_in_ns(void); // NTP-disciplined

// time at startup for debugging timing
extern uint64_t ns_time_at_startup, ns_time_at_last_debug_message;

// this is for reading an unsigned 32 bit number, such as an RTP timestamp
uint32_t uatoi(const char *nptr);

// this is for allowing us to cancel the whole program
extern pthread_t main_thread_id;

extern shairport_cfg config;
extern config_t config_file_stuff;
extern int emergency_exit;

int config_set_lookup_bool(config_t *cfg, char *where, int *dst);

int mkpath(const char *path, mode_t mode);

void shairport_shutdown();

extern sigset_t pselect_sigset;

extern pthread_mutex_t the_conn_lock;

#define conn_lock(arg)                                                                            \
  pthread_mutex_lock(&the_conn_lock);                                                             \
  arg;                                                                                            \
  pthread_mutex_unlock(&the_conn_lock);

// wait for the specified time in microseconds -- it checks every 20
// milliseconds
int sps_pthread_mutex_timedlock(pthread_mutex_t *mutex, useconds_t dally_time,
                                const char *debugmessage, int debuglevel);
// wait for the specified time, checking every 20 milliseconds, and block if it
// can't acquire the lock
int _debug_mutex_lock(pthread_mutex_t *mutex, useconds_t dally_time, const char *mutexName,
                      const char *filename, const int line, int debuglevel);

#define debug_mutex_lock(mu, t, d) _debug_mutex_lock(mu, t, #mu, __FILE__, __LINE__, d)

int _debug_mutex_unlock(pthread_mutex_t *mutex, const char *mutexName, const char *filename,
                        const int line, int debuglevel);

#define debug_mutex_unlock(mu, d) _debug_mutex_unlock(mu, #mu, __FILE__, __LINE__, d)

void pthread_cleanup_debug_mutex_unlock(void *arg);

#define pthread_cleanup_debug_mutex_lock(mu, t, d)                                                \
  if (_debug_mutex_lock(mu, t, #mu, __FILE__, __LINE__, d) == 0)                                  \
  pthread_cleanup_push(pthread_cleanup_debug_mutex_unlock, (void *)mu)

#define config_lock                                                                               \
  if (pthread_mutex_trylock(&config.lock) != 0) {                                                 \
    debug(1, "config_lock: cannot acquire config.lock");                                          \
  }

#define config_unlock pthread_mutex_unlock(&config.lock)

extern pthread_mutex_t r64_mutex;

#define r64_lock pthread_mutex_lock(&r64_mutex)

#define r64_unlock pthread_mutex_unlock(&r64_mutex)

char *get_version_string(); // mallocs a string space -- remember to free it
                            // afterwards

int64_t generate_zero_frames(char *outp, size_t number_of_frames, sps_format_t format,
                             int with_dither, int64_t random_number_in);

void malloc_cleanup(void *arg);

int string_update_with_size(char **str, int *flag, char *s, size_t len);

// from https://stackoverflow.com/questions/13663617/memdup-function-in-c, with
// thanks
void *memdup(const void *mem, size_t size);

int bind_socket_and_port(int type, int ip_family, const char *self_ip_address, uint32_t scope_id,
                         uint16_t *port, int *sock);

uint16_t bind_UDP_port(int ip_family, const char *self_ip_address, uint32_t scope_id, int *sock);

void socket_cleanup(void *arg);
void mutex_unlock(void *arg);
void mutex_cleanup(void *arg);
void cv_cleanup(void *arg);
void thread_cleanup(void *arg);

void plist_cleanup(void *arg);

char *debug_malloc_hex_cstring(void *packet, size_t nread);

// from https://stackoverflow.com/questions/13663617/memdup-function-in-c, with
// thanks allocates memory and copies the content to it analogous to strndup;
void *memdup(const void *mem, size_t size);

// the difference between two unsigned 32-bit modulo values as a signed 32-bit
// result now, if the two numbers are constrained to be within 2^(n-1)-1 of one
// another, we can use their as a signed 2^n bit number which will be positive
// if the first number is the same or "after" the second, and
// negative otherwise

int32_t mod32Difference(uint32_t a, uint32_t b);

int get_device_id(uint8_t *id, int int_length);

extern char git_version_string[];

#endif // _COMMON_H

#ifdef __cplusplus
}
#endif
