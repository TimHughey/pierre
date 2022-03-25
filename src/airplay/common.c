/*
 * Utility routines. This file is part of Shairport.
 * Copyright (c) James Laird 2013
 * The volume to attenuation function vol2attn copyright (c) Mike Brady 2014
 * Further changes and additions (c) Mike Brady 2014 -- 2021
 * All rights reserved.
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

#include "common.h"
#include "gitversion.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h> // PRIdPTR
#include <libgen.h>
#include <memory.h>
#include <poll.h>
#include <popt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <ifaddrs.h>

#include <netpacket/packet.h>

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <libdaemon/dlog.h>

config_t config_file_stuff;
int emergency_exit;
pthread_t main_thread_id;
uint64_t ns_time_at_startup, ns_time_at_last_debug_message;

// always lock use this when accessing the ns_time_at_last_debug_message
static pthread_mutex_t debug_timing_lock = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t the_conn_lock = PTHREAD_MUTEX_INITIALIZER;

const char *sps_format_description_string_array[] = {
    "unknown", "S8",     "U8",     "S16",     "S16_LE",  "S16_BE",
    "S24",     "S24_LE", "S24_BE", "S24_3LE", "S24_3BE", "S32",
    "S32_LE",  "S32_BE", "auto",   "invalid"};

const char *sps_format_description_string(sps_format_t format) {
  if (format <= SPS_FORMAT_AUTO)
    return sps_format_description_string_array[format];
  else
    return sps_format_description_string_array[SPS_FORMAT_INVALID];
}

// true if Shairport Sync is supposed to be sending output to the output device,
// false otherwise

static volatile int requested_connection_state_to_output = 1;

// this stuff is to direct logging to syslog via libdaemon or directly
// alternatively you can direct it to stderr using a command line option

static void (*sps_log)(int prio, const char *t, ...) = daemon_log;

void do_sps_log_to_stderr(__attribute__((unused)) int prio, const char *t,
                          ...) {
  char s[16384];
  va_list args;
  va_start(args, t);
  vsnprintf(s, sizeof(s), t, args);
  va_end(args);
  fprintf(stderr, "%s\n", s);
}

void do_sps_log_to_stdout(__attribute__((unused)) int prio, const char *t,
                          ...) {
  char s[16384];
  va_list args;
  va_start(args, t);
  vsnprintf(s, sizeof(s), t, args);
  va_end(args);
  fprintf(stdout, "%s\n", s);
}

void log_to_stderr() { sps_log = do_sps_log_to_stderr; }
void log_to_stdout() { sps_log = do_sps_log_to_stdout; }
void log_to_syslog() { sps_log = daemon_log; }

shairport_cfg config;

// accessors for multi-thread-access fields in the conn structure

double get_config_airplay_volume() {
  config_lock;
  double v = config.airplay_volume;
  config_unlock;
  return v;
}

void set_config_airplay_volume(double v) {
  config_lock;
  config.airplay_volume = v;
  config_unlock;
}

volatile int debuglev = 0;

sigset_t pselect_sigset;

int usleep_uncancellable(useconds_t usec) {
  int response;
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  response = usleep(usec);
  pthread_setcancelstate(oldState, NULL);
  return response;
}

static uint16_t UDPPortIndex = 0;

void resetFreeUDPPort() {
  debug(3, "Resetting UDP Port Suggestion to %u", config.udp_port_base);
  UDPPortIndex = 0;
}

uint16_t nextFreeUDPPort() {
  if (UDPPortIndex == 0)
    UDPPortIndex = config.udp_port_base;
  else if (UDPPortIndex == (config.udp_port_base + config.udp_port_range - 1))
    UDPPortIndex = config.udp_port_base +
                   3; // avoid wrapping back to the first three, as they can
                      // be assigned by resetFreeUDPPort without checking
  else
    UDPPortIndex++;
  return UDPPortIndex;
}

// if port is zero, pick any port
// otherwise, try the given port only
int bind_socket_and_port(int type, int ip_family, const char *self_ip_address,
                         uint32_t scope_id, uint16_t *port, int *sock) {
  int ret = 0; // no error
  int local_socket = socket(ip_family, type, 0);
  if (local_socket == -1)
    ret = errno;
  if (ret == 0) {
    SOCKADDR myaddr;
    memset(&myaddr, 0, sizeof(myaddr));
    if (ip_family == AF_INET) {
      struct sockaddr_in *sa = (struct sockaddr_in *)&myaddr;
      sa->sin_family = AF_INET;
      sa->sin_port = ntohs(*port);
      inet_pton(AF_INET, self_ip_address, &(sa->sin_addr));
      ret =
          bind(local_socket, (struct sockaddr *)sa, sizeof(struct sockaddr_in));
    }
#ifdef AF_INET6
    if (ip_family == AF_INET6) {
      struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)&myaddr;
      sa6->sin6_family = AF_INET6;
      sa6->sin6_port = ntohs(*port);
      inet_pton(AF_INET6, self_ip_address, &(sa6->sin6_addr));
      sa6->sin6_scope_id = scope_id;
      ret = bind(local_socket, (struct sockaddr *)sa6,
                 sizeof(struct sockaddr_in6));
    }
#endif
    if (ret < 0) {
      ret = errno;
      close(local_socket);
      char errorstring[1024];
      strerror_r(errno, (char *)errorstring, sizeof(errorstring));
      warn("error %d: \"%s\". Could not bind a port!", errno, errorstring);
    } else {
      uint16_t sport;
      SOCKADDR local;
      socklen_t local_len = sizeof(local);
      ret = getsockname(local_socket, (struct sockaddr *)&local, &local_len);
      if (ret < 0) {
        ret = errno;
        close(local_socket);
        char errorstring[1024];
        strerror_r(errno, (char *)errorstring, sizeof(errorstring));
        warn("error %d: \"%s\". Could not retrieve socket's port!", errno,
             errorstring);
      } else {
#ifdef AF_INET6
        if (local.SAFAMILY == AF_INET6) {
          struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)&local;
          sport = ntohs(sa6->sin6_port);
        } else
#endif
        {
          struct sockaddr_in *sa = (struct sockaddr_in *)&local;
          sport = ntohs(sa->sin_port);
        }
        *sock = local_socket;
        *port = sport;
      }
    }
  }
  return ret;
}

uint16_t bind_UDP_port(int ip_family, const char *self_ip_address,
                       uint32_t scope_id, int *sock) {
  // look for a port in the range, if any was specified.
  int ret = 0;

  int local_socket = socket(ip_family, SOCK_DGRAM, IPPROTO_UDP);
  if (local_socket == -1)
    die("Could not allocate a socket.");

  /*
    int val = 1;
    ret = setsockopt(local_socket, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    if (ret < 0) {
      char errorstring[1024];
      strerror_r(errno, (char *)errorstring, sizeof(errorstring));
      debug(1, "Error %d: \"%s\". Couldn't set SO_REUSEADDR");
    }
  */

  SOCKADDR myaddr;
  int tryCount = 0;
  uint16_t desired_port;
  do {
    tryCount++;
    desired_port = nextFreeUDPPort();
    memset(&myaddr, 0, sizeof(myaddr));
    if (ip_family == AF_INET) {
      struct sockaddr_in *sa = (struct sockaddr_in *)&myaddr;
      sa->sin_family = AF_INET;
      sa->sin_port = ntohs(desired_port);
      inet_pton(AF_INET, self_ip_address, &(sa->sin_addr));
      ret =
          bind(local_socket, (struct sockaddr *)sa, sizeof(struct sockaddr_in));
    }
#ifdef AF_INET6
    if (ip_family == AF_INET6) {
      struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)&myaddr;
      sa6->sin6_family = AF_INET6;
      sa6->sin6_port = ntohs(desired_port);
      inet_pton(AF_INET6, self_ip_address, &(sa6->sin6_addr));
      sa6->sin6_scope_id = scope_id;
      ret = bind(local_socket, (struct sockaddr *)sa6,
                 sizeof(struct sockaddr_in6));
    }
#endif

  } while ((ret < 0) && (errno == EADDRINUSE) && (desired_port != 0) &&
           (tryCount < config.udp_port_range));

  // debug(1,"UDP port chosen: %d.",desired_port);

  if (ret < 0) {
    close(local_socket);
    char errorstring[1024];
    strerror_r(errno, (char *)errorstring, sizeof(errorstring));
    die("error %d: \"%s\". Could not bind a UDP port! Check the udp_port_range "
        "is large enough -- "
        "it must be "
        "at least 3, and 10 or more is suggested -- or "
        "check for restrictive firewall settings or a bad router! UDP base is "
        "%u, range is %u and "
        "current suggestion is %u.",
        errno, errorstring, config.udp_port_base, config.udp_port_range,
        desired_port);
  }

  uint16_t sport;
  SOCKADDR local;
  socklen_t local_len = sizeof(local);
  getsockname(local_socket, (struct sockaddr *)&local, &local_len);
#ifdef AF_INET6
  if (local.SAFAMILY == AF_INET6) {
    struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)&local;
    sport = ntohs(sa6->sin6_port);
  } else
#endif
  {
    struct sockaddr_in *sa = (struct sockaddr_in *)&local;
    sport = ntohs(sa->sin_port);
  }
  *sock = local_socket;
  return sport;
}

int get_requested_connection_state_to_output() {
  return requested_connection_state_to_output;
}

void set_requested_connection_state_to_output(int v) {
  requested_connection_state_to_output = v;
}

char *generate_preliminary_string(char *buffer, size_t buffer_length,
                                  double tss, double tsl, const char *filename,
                                  const int linenumber, const char *prefix) {
  char *insertion_point = buffer;
  if (config.debugger_show_elapsed_time) {
    snprintf(insertion_point, buffer_length, "% 20.9f", tss);
    insertion_point = insertion_point + strlen(insertion_point);
  }
  if (config.debugger_show_relative_time) {
    snprintf(insertion_point, buffer_length, "% 20.9f", tsl);
    insertion_point = insertion_point + strlen(insertion_point);
  }
  if (config.debugger_show_file_and_line) {
    snprintf(insertion_point, buffer_length, " \"%s:%d\"", filename,
             linenumber);
    insertion_point = insertion_point + strlen(insertion_point);
  }
  if (prefix) {
    snprintf(insertion_point, buffer_length, "%s", prefix);
    insertion_point = insertion_point + strlen(insertion_point);
  }
  return insertion_point;
}

void _die(const char *filename, const int linenumber, const char *format, ...) {
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  char b[16384];
  b[0] = 0;
  char *s;
  if (debuglev) {
    pthread_mutex_lock(&debug_timing_lock);
    uint64_t time_now = get_absolute_time_in_ns();
    uint64_t time_since_start = time_now - ns_time_at_startup;
    uint64_t time_since_last_debug_message =
        time_now - ns_time_at_last_debug_message;
    ns_time_at_last_debug_message = time_now;
    pthread_mutex_unlock(&debug_timing_lock);
    s = generate_preliminary_string(
        b, sizeof(b), 1.0 * time_since_start / 1000000000,
        1.0 * time_since_last_debug_message / 1000000000, filename, linenumber,
        " *fatal error: ");
  } else {
    strncpy(b, "fatal error: ", sizeof(b));
    s = b + strlen(b);
  }
  va_list args;
  va_start(args, format);
  vsnprintf(s, sizeof(b) - (s - b), format, args);
  va_end(args);
  sps_log(LOG_ERR, "%s", b);
  pthread_setcancelstate(oldState, NULL);
  emergency_exit = 1;
  exit(EXIT_FAILURE);
}

void _warn(const char *filename, const int linenumber, const char *format,
           ...) {
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  char b[16384];
  b[0] = 0;
  char *s;
  if (debuglev) {
    pthread_mutex_lock(&debug_timing_lock);
    uint64_t time_now = get_absolute_time_in_ns();
    uint64_t time_since_start = time_now - ns_time_at_startup;
    uint64_t time_since_last_debug_message =
        time_now - ns_time_at_last_debug_message;
    ns_time_at_last_debug_message = time_now;
    pthread_mutex_unlock(&debug_timing_lock);
    s = generate_preliminary_string(
        b, sizeof(b), 1.0 * time_since_start / 1000000000,
        1.0 * time_since_last_debug_message / 1000000000, filename, linenumber,
        " *warning: ");
  } else {
    strncpy(b, "warning: ", sizeof(b));
    s = b + strlen(b);
  }
  va_list args;
  va_start(args, format);
  vsnprintf(s, sizeof(b) - (s - b), format, args);
  va_end(args);
  sps_log(LOG_WARNING, "%s", b);
  pthread_setcancelstate(oldState, NULL);
}

void _debug(const char *filename, const int linenumber, int level,
            const char *format, ...) {
  if (level > debuglev)
    return;
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  char b[16384];
  b[0] = 0;
  pthread_mutex_lock(&debug_timing_lock);
  uint64_t time_now = get_absolute_time_in_ns();
  uint64_t time_since_start = time_now - ns_time_at_startup;
  uint64_t time_since_last_debug_message =
      time_now - ns_time_at_last_debug_message;
  ns_time_at_last_debug_message = time_now;
  pthread_mutex_unlock(&debug_timing_lock);
  char *s = generate_preliminary_string(
      b, sizeof(b), 1.0 * time_since_start / 1000000000,
      1.0 * time_since_last_debug_message / 1000000000, filename, linenumber,
      " ");
  va_list args;
  va_start(args, format);
  vsnprintf(s, sizeof(b) - (s - b), format, args);
  va_end(args);
  sps_log(LOG_INFO, b); // LOG_DEBUG is hard to read on macOS terminal
  pthread_setcancelstate(oldState, NULL);
}

void _inform(const char *filename, const int linenumber, const char *format,
             ...) {
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  char b[16384];
  b[0] = 0;
  char *s;
  if (debuglev) {
    pthread_mutex_lock(&debug_timing_lock);
    uint64_t time_now = get_absolute_time_in_ns();
    uint64_t time_since_start = time_now - ns_time_at_startup;
    uint64_t time_since_last_debug_message =
        time_now - ns_time_at_last_debug_message;
    ns_time_at_last_debug_message = time_now;
    pthread_mutex_unlock(&debug_timing_lock);
    s = generate_preliminary_string(
        b, sizeof(b), 1.0 * time_since_start / 1000000000,
        1.0 * time_since_last_debug_message / 1000000000, filename, linenumber,
        " ");
  } else {
    s = b;
  }
  va_list args;
  va_start(args, format);
  vsnprintf(s, sizeof(b) - (s - b), format, args);
  va_end(args);
  sps_log(LOG_INFO, "%s", b);
  pthread_setcancelstate(oldState, NULL);
}

// The following two functions are adapted slightly and with thanks from
// Jonathan Leffler's sample code at
// https://stackoverflow.com/questions/675039/how-can-i-create-directory-tree-in-c-linux

int do_mkdir(const char *path, mode_t mode) {
  struct stat st;
  int status = 0;

  if (stat(path, &st) != 0) {
    /* Directory does not exist. EEXIST for race condition */
    if (mkdir(path, mode) != 0 && errno != EEXIST)
      status = -1;
  } else if (!S_ISDIR(st.st_mode)) {
    errno = ENOTDIR;
    status = -1;
  }

  return (status);
}

// mkpath - ensure all directories in path exist
// Algorithm takes the pessimistic view and works top-down to ensure
// each directory in path exists, rather than optimistically creating
// the last element and working backwards.

int mkpath(const char *path, mode_t mode) {
  char *pp;
  char *sp;
  int status;
  char *copypath = strdup(path);

  status = 0;
  pp = copypath;
  while (status == 0 && (sp = strchr(pp, '/')) != 0) {
    if (sp != pp) {
      /* Neither root nor double slash in path */
      *sp = '\0';
      status = do_mkdir(copypath, mode);
      *sp = '/';
    }
    pp = sp + 1;
  }
  if (status == 0)
    status = do_mkdir(path, mode);
  free(copypath);
  return (status);
}

char *base64_enc(uint8_t *input, int length) {
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  BIO *bmem, *b64;
  BUF_MEM *bptr;
  b64 = BIO_new(BIO_f_base64());
  bmem = BIO_new(BIO_s_mem());
  b64 = BIO_push(b64, bmem);
  BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
  BIO_write(b64, input, length);
  (void)BIO_flush(b64);
  BIO_get_mem_ptr(b64, &bptr);

  char *buf = (char *)malloc(bptr->length);
  if (buf == NULL)
    die("could not allocate memory for buf in base64_enc");
  if (bptr->length) {
    memcpy(buf, bptr->data, bptr->length - 1);
    buf[bptr->length - 1] = 0;
  }

  BIO_free_all(b64);

  pthread_setcancelstate(oldState, NULL);
  return buf;
}

uint8_t *base64_dec(char *input, int *outlen) {
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  BIO *bmem, *b64;
  int inlen = strlen(input);

  b64 = BIO_new(BIO_f_base64());
  BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
  bmem = BIO_new(BIO_s_mem());
  b64 = BIO_push(b64, bmem);

  // Apple cut the padding off their challenges; restore it
  BIO_write(bmem, input, inlen);
  while (inlen++ & 3)
    BIO_write(bmem, "=", 1);
  (void)BIO_flush(bmem);

  int bufsize = strlen(input) * 3 / 4 + 1;
  uint8_t *buf = malloc(bufsize);
  int nread;

  nread = BIO_read(b64, buf, bufsize);

  BIO_free_all(b64);

  *outlen = nread;
  pthread_setcancelstate(oldState, NULL);
  return buf;
}

static char super_secret_key[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "MIIEpQIBAAKCAQEA59dE8qLieItsH1WgjrcFRKj6eUWqi+bGLOX1HL3U3GhC/j0Qg90u3sG/"
    "1CUt\n"
    "wC5vOYvfDmFI6oSFXi5ELabWJmT2dKHzBJKa3k9ok+"
    "8t9ucRqMd6DZHJ2YCCLlDRKSKv6kDqnw4U\n"
    "wPdpOMXziC/AMj3Z/lUVX1G7WSHCAWKf1zNS1eLvqr+boEjXuBOitnZ/"
    "bDzPHrTOZz0Dew0uowxf\n"
    "/+sG+NCK3eQJVxqcaJ/"
    "vEHKIVd2M+5qL71yJQ+87X6oV3eaYvt3zWZYD6z5vYTcrtij2VZ9Zmni/\n"
    "UAaHqn9JdsBWLUEpVviYnhimNVvYFZeCXg/"
    "IdTQ+x4IRdiXNv5hEewIDAQABAoIBAQDl8Axy9XfW\n"
    "BLmkzkEiqoSwF0PsmVrPzH9KsnwLGH+"
    "QZlvjWd8SWYGN7u1507HvhF5N3drJoVU3O14nDY4TFQAa\n"
    "LlJ9VM35AApXaLyY1ERrN7u9ALKd2LUwYhM7Km539O4yUFYikE2nIPscEsA5ltpxOgUGCY7b7e"
    "z5\n"
    "NtD6nL1ZKauw7aNXmVAvmJTcuPxWmoktF3gDJKK2wxZuNGcJE0uFQEG4Z3BrWP7yoNuSK3dii2"
    "jm\n"
    "lpPHr0O/KnPQtzI3eguhe0TwUem/eYSdyzMyVx/"
    "YpwkzwtYL3sR5k0o9rKQLtvLzfAqdBxBurciz\n"
    "aaA/"
    "L0HIgAmOit1GJA2saMxTVPNhAoGBAPfgv1oeZxgxmotiCcMXFEQEWflzhWYTsXrhUIuz5jFu\n"
    "a39GLS99ZEErhLdrwj8rDDViRVJ5skOp9zFvlYAHs0xh92ji1E7V/"
    "ysnKBfsMrPkk5KSKPrnjndM\n"
    "oPdevWnVkgJ5jxFuNgxkOLMuG9i53B4yMvDTCRiIPMQ++N2iLDaRAoGBAO9v//"
    "mU8eVkQaoANf0Z\n"
    "oMjW8CN4xwWA2cSEIHkd9AfFkftuv8oyLDCG3ZAf0vrhrrtkrfa7ef+"
    "AUb69DNggq4mHQAYBp7L+\n"
    "k5DKzJrKuO0r+R0YbY9pZD1+/g9dVt91d6LQNepUE/"
    "yY2PP5CNoFmjedpLHMOPFdVgqDzDFxU8hL\n"
    "AoGBANDrr7xAJbqBjHVwIzQ4To9pb4BNeqDndk5Qe7fT3+/H1njGaC0/"
    "rXE0Qb7q5ySgnsCb3DvA\n"
    "cJyRM9SJ7OKlGt0FMSdJD5KG0XPIpAVNwgpXXH5MDJg09KHeh0kXo+"
    "QA6viFBi21y340NonnEfdf\n"
    "54PX4ZGS/"
    "Xac1UK+pLkBB+zRAoGAf0AY3H3qKS2lMEI4bzEFoHeK3G895pDaK3TFBVmD7fV0Zhov\n"
    "17fegFPMwOII8MisYm9ZfT2Z0s5Ro3s5rkt+nvLAdfC/"
    "PYPKzTLalpGSwomSNYJcB9HNMlmhkGzc\n"
    "1JnLYT4iyUyx6pcZBmCd8bD0iwY/"
    "FzcgNDaUmbX9+XDvRA0CgYEAkE7pIPlE71qvfJQgoA9em0gI\n"
    "LAuE4Pu13aKiJnfft7hIjbK+"
    "5kyb3TysZvoyDnb3HOKvInK7vXbKuU4ISgxB2bB3HcYzQMGsz1qJ\n"
    "2gG0N5hvJpzwwhbhXqFKA4zaaSrw622wDniAK5MlIE0tIAKKP4yxNGjoD2QYjhBGuhvkWKY=\n"
    "-----END RSA PRIVATE KEY-----\0";

uint8_t *rsa_apply(uint8_t *input, int inlen, int *outlen, int mode) {
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  RSA *rsa = NULL;
  if (!rsa) {
    BIO *bmem = BIO_new_mem_buf(super_secret_key, -1);
    rsa = PEM_read_bio_RSAPrivateKey(bmem, NULL, NULL, NULL);
    BIO_free(bmem);
  }

  uint8_t *out = malloc(RSA_size(rsa));
  switch (mode) {
  case RSA_MODE_AUTH:
    *outlen = RSA_private_encrypt(inlen, input, out, rsa, RSA_PKCS1_PADDING);
    break;
  case RSA_MODE_KEY:
    *outlen =
        RSA_private_decrypt(inlen, input, out, rsa, RSA_PKCS1_OAEP_PADDING);
    break;
  default:
    die("bad rsa mode");
  }
  RSA_free(rsa);
  pthread_setcancelstate(oldState, NULL);
  return out;
}

int config_set_lookup_bool(config_t *cfg, char *where, int *dst) {
  const char *str = 0;
  if (config_lookup_string(cfg, where, &str)) {
    if (strcasecmp(str, "no") == 0) {
      (*dst) = 0;
      return 1;
    } else if (strcasecmp(str, "yes") == 0) {
      (*dst) = 1;
      return 1;
    } else {
      die("Invalid %s option choice \"%s\". It should be \"yes\" or \"no\"",
          where, str);
      return 0;
    }
  } else {
    return 0;
  }
}

// this is for reading an unsigned 32 bit number, such as an RTP timestamp

uint32_t uatoi(const char *nptr) {
  uint64_t llint = atoll(nptr);
  uint32_t r = llint;
  return r;
}

double flat_vol2attn(double vol, long max_db, long min_db) {
  double vol_setting = min_db; // if all else fails, set this, for safety

  if ((vol <= 0.0) && (vol >= -30.0)) {
    vol_setting = ((max_db - min_db) * (30.0 + vol) / 30) + min_db;
    // debug(2, "Linear profile Volume Setting: %f in range %ld to %ld.",
    // vol_setting, min_db, max_db);
  } else if (vol != -144.0) {
    debug(1,
          "Linear volume request value %f is out of range: should be from 0.0 "
          "to -30.0 or -144.0.",
          vol);
  }
  return vol_setting;
}
// Given a volume (0 to -30) and high and low attenuations available in the
// mixer in dB, return an attenuation depending on the volume and the function's
// transfer function See http://tangentsoft.net/audio/atten.html for data on
// good attenuators. We want a smooth attenuation function, like, for example,
// the ALPS RK27 Potentiometer transfer functions referred to at the link above.

// Note that the max_db and min_db are given as dB*100

double vol2attn(double vol, long max_db, long min_db) {

  // We use a little coordinate geometry to build a transfer function from the
  // volume passed in to the device's dynamic range. (See the diagram in the
  // documents folder.) The x axis is the "volume in" which will be from -30 to
  // 0. The y axis will be the "volume out" which will be from the bottom of the
  // range to the top. We build the transfer function from one or more lines. We
  // characterise each line with two numbers: the first is where on x the line
  // starts when y=0 (x can be from 0 to -30); the second is where on y the line
  // stops when when x is -30. thus, if the line was characterised as {0,-30},
  // it would be an identity transfer. Assuming, for example, a dynamic range of
  // lv=-60 to hv=0 Typically we'll use three lines -- a three order transfer
  // function First: {0,30} giving a gentle slope -- the 30 comes from half the
  // dynamic range Second: {-5,-30-(lv+30)/2} giving a faster slope from y=0 at
  // x=-12 to y=-42.5 at x=-30 Third: {-17,lv} giving a fast slope from y=0 at
  // x=-19 to y=-60 at x=-30

#define order 3

  double vol_setting = 0;

  if ((vol <= 0.0) && (vol >= -30.0)) {
    long range_db = max_db - min_db; // this will be a positive number
    // debug(1,"Volume min %ddB, max %ddB, range %ddB.",min_db,max_db,range_db);
    // double first_slope = -3000.0; // this is the slope of the attenuation at
    // the high end -- 30dB for the full rotation.
    double first_slope =
        -range_db / 2; // this is the slope of the attenuation at the high end
                       // -- 30dB for the full rotation.
    if (-range_db > first_slope)
      first_slope = range_db;
    double lines[order][2] = {{0, first_slope},
                              {-5, first_slope - (range_db + first_slope) / 2},
                              {-17, -range_db}};
    int i;
    for (i = 0; i < order; i++) {
      if (vol <= lines[i][0]) {
        if ((-30 - lines[i][0]) == 0.0)
          die("(-30 - lines[%d][0]) == 0.0!", i);
        double tvol = lines[i][1] * (vol - lines[i][0]) / (-30 - lines[i][0]);
        // debug(1,"On line %d, end point of %f, input vol %f yields output vol
        // %f.",i,lines[i][1],vol,tvol);
        if (tvol < vol_setting)
          vol_setting = tvol;
      }
    }
    vol_setting += max_db;
  } else if (vol != -144.0) {
    debug(1,
          "Volume request value %f is out of range: should be from 0.0 to "
          "-30.0 or -144.0.",
          vol);
    vol_setting = min_db; // for safety, return the lowest setting...
  } else {
    vol_setting = min_db; // for safety, return the lowest setting...
  }
  // debug(1,"returning an attenuation of %f.",vol_setting);
  // debug(2, "Standard profile Volume Setting for Airplay vol %f: %f in range
  // %ld to %ld.", vol,
  //      vol_setting, min_db, max_db);
  return vol_setting;
}

uint64_t get_monotonic_time_in_ns() {
  uint64_t time_now_ns;

  struct timespec tn;
  // can't use CLOCK_MONOTONIC_RAW as it's not implemented in OpenWrt
  // CLOCK_REALTIME because PTP uses it.
  clock_gettime(CLOCK_MONOTONIC, &tn);
  uint64_t tnnsec = tn.tv_sec;
  tnnsec = tnnsec * 1000000000;
  uint64_t tnjnsec = tn.tv_nsec;
  time_now_ns = tnnsec + tnjnsec;

  return time_now_ns;
}

// all these clock things are now in macOS now since 10.13 (September 2017).
// Must update...
uint64_t get_monotonic_raw_time_in_ns() {
  // CLOCK_MONOTONIC_RAW in FreeBSD etc, monotonic in MacOSX
  uint64_t time_now_ns;

  struct timespec tn;
  clock_gettime(CLOCK_MONOTONIC_RAW, &tn);
  uint64_t tnnsec = tn.tv_sec;
  tnnsec = tnnsec * 1000000000;
  uint64_t tnjnsec = tn.tv_nsec;
  time_now_ns = tnnsec + tnjnsec;

  return time_now_ns;
}

uint64_t get_realtime_in_ns() {
  uint64_t time_now_ns;

  struct timespec tn;
  // can't use CLOCK_MONOTONIC_RAW as it's not implemented in OpenWrt
  // CLOCK_REALTIME because PTP uses it.
  clock_gettime(CLOCK_REALTIME, &tn);
  uint64_t tnnsec = tn.tv_sec;
  tnnsec = tnnsec * 1000000000;
  uint64_t tnjnsec = tn.tv_nsec;
  time_now_ns = tnnsec + tnjnsec;
  return time_now_ns;
}

uint64_t get_absolute_time_in_ns() {
  // CLOCK_MONOTONIC_RAW in FreeBSD etc, monotonic in MacOSX
  uint64_t time_now_ns;

  struct timespec tn;
  clock_gettime(CLOCK_MONOTONIC_RAW, &tn);
  uint64_t tnnsec = tn.tv_sec;
  tnnsec = tnnsec * 1000000000;
  uint64_t tnjnsec = tn.tv_nsec;
  time_now_ns = tnnsec + tnjnsec;

  return time_now_ns;
}

int try_to_open_pipe_for_writing(const char *pathname) {
  // tries to open the pipe in non-blocking mode first.
  // if it succeeds, it sets it to blocking.
  // if not, it returns -1.

  int fdis = open(pathname,
                  O_WRONLY | O_NONBLOCK); // open it in non blocking mode first

  // we check that it's not a "real" error. From the "man 2 open" page:
  // "ENXIO  O_NONBLOCK | O_WRONLY is set, the named file is a FIFO, and no
  // process has the FIFO open for reading." Which is okay. This is checked by
  // the caller.

  if (fdis >= 0) {
    // now we switch to blocking mode
    int flags = fcntl(fdis, F_GETFL);
    if (flags == -1) {
      char errorstring[1024];
      strerror_r(errno, (char *)errorstring, sizeof(errorstring));
      debug(1,
            "try_to_open_pipe -- error %d (\"%s\") getting flags of pipe: "
            "\"%s\".",
            errno, (char *)errorstring, pathname);
    } else {
      flags = fcntl(fdis, F_SETFL, flags & ~O_NONBLOCK);
      if (flags == -1) {
        char errorstring[1024];
        strerror_r(errno, (char *)errorstring, sizeof(errorstring));
        debug(1,
              "try_to_open_pipe -- error %d (\"%s\") unsetting NONBLOCK of "
              "pipe: \"%s\".",
              errno, (char *)errorstring, pathname);
      }
    }
  }
  return fdis;
}

/* from
 * http://coding.debuntu.org/c-implementing-str_replace-replace-all-occurrences-substring#comment-722
 */

char *str_replace(const char *string, const char *substr,
                  const char *replacement) {
  char *tok = NULL;
  char *newstr = NULL;
  char *oldstr = NULL;
  char *head = NULL;

  /* if either substr or replacement is NULL, duplicate string a let caller
   * handle it */
  if (substr == NULL || replacement == NULL)
    return strdup(string);
  newstr = strdup(string);
  head = newstr;
  if (head) {
    while ((tok = strstr(head, substr))) {
      oldstr = newstr;
      newstr =
          malloc(strlen(oldstr) - strlen(substr) + strlen(replacement) + 1);
      /*failed to alloc mem, free old string and return NULL */
      if (newstr == NULL) {
        free(oldstr);
        return NULL;
      }
      memcpy(newstr, oldstr, tok - oldstr);
      memcpy(newstr + (tok - oldstr), replacement, strlen(replacement));
      memcpy(newstr + (tok - oldstr) + strlen(replacement),
             tok + strlen(substr),
             strlen(oldstr) - strlen(substr) - (tok - oldstr));
      memset(newstr + strlen(oldstr) - strlen(substr) + strlen(replacement), 0,
             1);
      /* move back head right after the last replacement */
      head = newstr + (tok - oldstr) + strlen(replacement);
      free(oldstr);
    }
  } else {
    die("failed to allocate memory in str_replace.");
  }
  return newstr;
}

/* from http://burtleburtle.net/bob/rand/smallprng.html */

// this is not thread-safe, so we need a mutex on it to use it properly.
// always lock use this when accessing the fp_time_at_last_debug_message

pthread_mutex_t r64_mutex = PTHREAD_MUTEX_INITIALIZER;

// typedef uint64_t u8;
typedef struct ranctx {
  uint64_t a;
  uint64_t b;
  uint64_t c;
  uint64_t d;
} ranctx;

static struct ranctx rx;

#define rot(x, k) (((x) << (k)) | ((x) >> (64 - (k))))
uint64_t ranval(ranctx *x) {
  uint64_t e = x->a - rot(x->b, 7);
  x->a = x->b ^ rot(x->c, 13);
  x->b = x->c + rot(x->d, 37);
  x->c = x->d + e;
  x->d = e + x->a;
  return x->d;
}

void raninit(ranctx *x, uint64_t seed) {
  uint64_t i;
  x->a = 0xf1ea5eed, x->b = x->c = x->d = seed;
  for (i = 0; i < 20; ++i) {
    (void)ranval(x);
  }
}

void r64init(uint64_t seed) { raninit(&rx, seed); }

uint64_t r64u() { return (ranval(&rx)); }

int64_t r64i() { return (ranval(&rx) >> 1); }

uint32_t
nctohl(const uint8_t *p) { // read 4 characters from *p and do ntohl on them
  // this is to avoid possible aliasing violations
  uint32_t holder;
  memcpy(&holder, p, sizeof(holder));
  return ntohl(holder);
}

uint16_t nctohs(const uint8_t *p) {
  // read 2 characters from *p and do ntohs on them
  // this is to avoid possible aliasing violations
  uint16_t holder;
  memcpy(&holder, p, sizeof(holder));
  return ntohs(holder);
}

uint64_t nctoh64(const uint8_t *p) {
  uint32_t landing = nctohl(p); // get the high order 32 bits
  uint64_t vl = landing;
  vl = vl << 32; // shift them into the correct location
  landing = nctohl(p + sizeof(uint32_t)); // and the low order 32 bits
  uint64_t ul = landing;
  vl = vl + ul;
  return vl;
}

pthread_mutex_t barrier_mutex = PTHREAD_MUTEX_INITIALIZER;

void memory_barrier() {
  pthread_mutex_lock(&barrier_mutex);
  pthread_mutex_unlock(&barrier_mutex);
}

void sps_nanosleep(const time_t sec, const long nanosec) {
  struct timespec req, rem;
  int result;
  req.tv_sec = sec;
  req.tv_nsec = nanosec;
  do {
    result = nanosleep(&req, &rem);
    rem = req;
  } while ((result == -1) && (errno == EINTR));
  if (result == -1)
    debug(1, "Error in sps_nanosleep of %d sec and %ld nanoseconds: %d.", sec,
          nanosec, errno);
}

int sps_pthread_mutex_timedlock(pthread_mutex_t *mutex, useconds_t dally_time,
                                const char *debugmessage, int debuglevel) {

  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);

  struct timespec timeoutTime;
  uint64_t wait_until_time = dally_time * 1000; // to nanoseconds
  uint64_t start_time = get_realtime_in_ns();   // this is from CLOCK_REALTIME
  wait_until_time = wait_until_time + start_time;
  uint64_t wait_until_sec = wait_until_time / 1000000000;
  uint64_t wait_until_nsec = wait_until_time % 1000000000;
  timeoutTime.tv_sec = wait_until_sec;
  timeoutTime.tv_nsec = wait_until_nsec;

  int r = pthread_mutex_timedlock(mutex, &timeoutTime);
  uint64_t et = get_realtime_in_ns() - start_time;

  if ((debuglevel != 0) && (r != 0) && (debugmessage != NULL)) {
    char errstr[1000];
    if (r == ETIMEDOUT)
      debug(debuglevel,
            "Timed out waiting for a mutex, having waited %f seconds with a "
            "maximum "
            "waiting time of %f seconds. \"%s\".",
            (1.0 * et) / 1000000000, dally_time * 0.000001, debugmessage);
    else
      debug(debuglevel, "error %d: \"%s\" waiting for a mutex: \"%s\".", r,
            strerror_r(r, errstr, sizeof(errstr)), debugmessage);
  }
  pthread_setcancelstate(oldState, NULL);
  return r;
}

int _debug_mutex_lock(pthread_mutex_t *mutex, useconds_t dally_time,
                      const char *mutexname, const char *filename,
                      const int line, int debuglevel) {
  if ((debuglevel > debuglev) || (debuglevel == 0))
    return pthread_mutex_lock(mutex);
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  uint64_t time_at_start = get_absolute_time_in_ns();
  char dstring[1000];
  memset(dstring, 0, sizeof(dstring));
  snprintf(dstring, sizeof(dstring), "%s:%d", filename, line);
  if (debuglevel != 0)
    debug(3, "mutex_lock \"%s\" at \"%s\".", mutexname,
          dstring); // only if you really ask for it!
  int result =
      sps_pthread_mutex_timedlock(mutex, dally_time, dstring, debuglevel);
  if (result == ETIMEDOUT) {
    result = pthread_mutex_lock(mutex);
    uint64_t time_delay = get_absolute_time_in_ns() - time_at_start;
    debug(debuglevel,
          "Mutex_lock \"%s\" at \"%s\" expected max wait: %0.9f, actual wait: "
          "%0.9f sec.",
          mutexname, dstring, (1.0 * dally_time) / 1000000,
          0.000000001 * time_delay);
  }
  pthread_setcancelstate(oldState, NULL);
  return result;
}

int _debug_mutex_unlock(pthread_mutex_t *mutex, const char *mutexname,
                        const char *filename, const int line, int debuglevel) {
  if ((debuglevel > debuglev) || (debuglevel == 0))
    return pthread_mutex_unlock(mutex);
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  char dstring[1000];
  char errstr[512];
  memset(dstring, 0, sizeof(dstring));
  snprintf(dstring, sizeof(dstring), "%s:%d", filename, line);
  debug(debuglevel, "mutex_unlock \"%s\" at \"%s\".", mutexname, dstring);
  int r = pthread_mutex_unlock(mutex);
  if ((debuglevel != 0) && (r != 0))
    debug(1, "error %d: \"%s\" unlocking mutex \"%s\" at \"%s\".", r,
          strerror_r(r, errstr, sizeof(errstr)), mutexname, dstring);
  pthread_setcancelstate(oldState, NULL);
  return r;
}

void malloc_cleanup(void *arg) {
  // debug(1, "malloc cleanup called.");
  free(arg);
}

void plist_cleanup(void *arg) {
  // debug(1, "plist cleanup called.");
  plist_free((plist_t)arg);
}

void socket_cleanup(void *arg) {
  intptr_t fdp = (intptr_t)arg;
  debug(3, "socket_cleanup called for socket: %" PRIdPTR ".", fdp);
  close(fdp);
}

void cv_cleanup(void *arg) {
  // debug(1, "cv_cleanup called.");
  pthread_cond_t *cv = (pthread_cond_t *)arg;
  pthread_cond_destroy(cv);
}

void mutex_cleanup(void *arg) {
  // debug(1, "mutex_cleanup called.");
  pthread_mutex_t *mutex = (pthread_mutex_t *)arg;
  pthread_mutex_destroy(mutex);
}

void mutex_unlock(void *arg) { pthread_mutex_unlock((pthread_mutex_t *)arg); }

void thread_cleanup(void *arg) {
  debug(3, "thread_cleanup called.");
  pthread_t *thread = (pthread_t *)arg;
  pthread_cancel(*thread);
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  pthread_join(*thread, NULL);
  pthread_setcancelstate(oldState, NULL);
  debug(3, "thread_cleanup done.");
}

void pthread_cleanup_debug_mutex_unlock(void *arg) {
  pthread_mutex_unlock((pthread_mutex_t *)arg);
}

char *get_version_string() {
  char *version_string = malloc(1024);
  if (version_string) {
    if (git_version_string[0] != '\0')
      strcpy(version_string, git_version_string);
    else
      strcpy(version_string, PACKAGE_VERSION);

    strcat(version_string, "-AirPlay2");
    strcat(version_string, "-libdaemon");

    strcat(version_string, "-OpenSSL");

    strcat(version_string, "-Avahi");
    strcat(version_string, "-dummy");

    strcat(version_string, "-sysconfdir:");
    strcat(version_string, SYSCONFDIR);
  }
  return version_string;
}

int64_t generate_zero_frames(char *outp, size_t number_of_frames,
                             sps_format_t format, int with_dither,
                             int64_t random_number_in) {
  // return the last random number used
  // assuming the buffer has been assigned

  // add a TPDF dither -- see
  // http://educypedia.karadimov.info/library/DitherExplained.pdf
  // and the discussion around
  // https://www.hydrogenaud.io/forums/index.php?showtopic=16963&st=25

  // I think, for a 32 --> 16 bits, the range of
  // random numbers needs to be from -2^16 to 2^16, i.e. from -65536 to 65536
  // inclusive, not from -32768 to +32767

  // Actually, what would be generated here is from -65535 to 65535, i.e. one
  // less on the limits.

  // See the original paper at
  // http://www.ece.rochester.edu/courses/ECE472/resources/Papers/Lipshitz_1992.pdf
  // by Lipshitz, Wannamaker and Vanderkooy, 1992.

  int64_t dither_mask = 0;
  switch (format) {
  case SPS_FORMAT_S32:
  case SPS_FORMAT_S32_LE:
  case SPS_FORMAT_S32_BE:
    dither_mask = (int64_t)1 << (64 - 32);
    break;
  case SPS_FORMAT_S24:
  case SPS_FORMAT_S24_LE:
  case SPS_FORMAT_S24_BE:
  case SPS_FORMAT_S24_3LE:
  case SPS_FORMAT_S24_3BE:
    dither_mask = (int64_t)1 << (64 - 24);
    break;
  case SPS_FORMAT_S16:
  case SPS_FORMAT_S16_LE:
  case SPS_FORMAT_S16_BE:
    dither_mask = (int64_t)1 << (64 - 16);
    break;

  case SPS_FORMAT_S8:

  case SPS_FORMAT_U8:
    dither_mask = (int64_t)1 << (64 - 8);
    break;
  case SPS_FORMAT_UNKNOWN:
    die("Unexpected SPS_FORMAT_UNKNOWN while calculating dither mask.");
    break;
  case SPS_FORMAT_AUTO:
    die("Unexpected SPS_FORMAT_AUTO while calculating dither mask.");
    break;
  case SPS_FORMAT_INVALID:
    die("Unexpected SPS_FORMAT_INVALID while calculating dither mask.");
    break;
  }
  dither_mask -= 1;

  int64_t previous_random_number = random_number_in;
  char *p = outp;
  size_t sample_number;
  r64_lock; // the random number generator is not thread safe, so we need to
            // lock it while using it
  for (sample_number = 0; sample_number < number_of_frames * 2;
       sample_number++) {

    int64_t hyper_sample = 0;
    int64_t r = r64i();

    int64_t tpdf = (r & dither_mask) - (previous_random_number & dither_mask);

    // add dither if permitted -- no need to check for clipping, as the sample
    // is, uh, zero

    if (with_dither != 0)
      hyper_sample += tpdf;

    // move the result to the desired position in the int64_t
    char *op = p;
    int sample_length; // this is the length of the sample

    switch (format) {
    case SPS_FORMAT_S32:
      hyper_sample >>= (64 - 32);
      *(int32_t *)op = hyper_sample;
      sample_length = 4;
      break;
    case SPS_FORMAT_S32_LE:
      *op++ = (uint8_t)(hyper_sample >> (64 - 32)); // 32 bits, ls byte
      *op++ = (uint8_t)(hyper_sample >>
                        (64 - 32 + 8)); // 32 bits, less significant middle byte
      *op++ =
          (uint8_t)(hyper_sample >>
                    (64 - 32 + 16)); // 32 bits, more significant middle byte
      *op = (uint8_t)(hyper_sample >> (64 - 32 + 24)); // 32 bits, ms byte
      sample_length = 4;
      break;
    case SPS_FORMAT_S32_BE:
      *op++ = (uint8_t)(hyper_sample >> (64 - 32 + 24)); // 32 bits, ms byte
      *op++ =
          (uint8_t)(hyper_sample >>
                    (64 - 32 + 16)); // 32 bits, more significant middle byte
      *op++ = (uint8_t)(hyper_sample >>
                        (64 - 32 + 8)); // 32 bits, less significant middle byte
      *op = (uint8_t)(hyper_sample >> (64 - 32)); // 32 bits, ls byte
      sample_length = 4;
      break;
    case SPS_FORMAT_S24_3LE:
      *op++ = (uint8_t)(hyper_sample >> (64 - 24));     // 24 bits, ls byte
      *op++ = (uint8_t)(hyper_sample >> (64 - 24 + 8)); // 24 bits, middle byte
      *op = (uint8_t)(hyper_sample >> (64 - 24 + 16));  // 24 bits, ms byte
      sample_length = 3;
      break;
    case SPS_FORMAT_S24_3BE:
      *op++ = (uint8_t)(hyper_sample >> (64 - 24 + 16)); // 24 bits, ms byte
      *op++ = (uint8_t)(hyper_sample >> (64 - 24 + 8));  // 24 bits, middle byte
      *op = (uint8_t)(hyper_sample >> (64 - 24));        // 24 bits, ls byte
      sample_length = 3;
      break;
    case SPS_FORMAT_S24:
      hyper_sample >>= (64 - 24);
      *(int32_t *)op = hyper_sample;
      sample_length = 4;
      break;
    case SPS_FORMAT_S24_LE:
      *op++ = (uint8_t)(hyper_sample >> (64 - 24));      // 24 bits, ls byte
      *op++ = (uint8_t)(hyper_sample >> (64 - 24 + 8));  // 24 bits, middle byte
      *op++ = (uint8_t)(hyper_sample >> (64 - 24 + 16)); // 24 bits, ms byte
      *op = 0;
      sample_length = 4;
      break;
    case SPS_FORMAT_S24_BE:
      *op++ = 0;
      *op++ = (uint8_t)(hyper_sample >> (64 - 24 + 16)); // 24 bits, ms byte
      *op++ = (uint8_t)(hyper_sample >> (64 - 24 + 8));  // 24 bits, middle byte
      *op = (uint8_t)(hyper_sample >> (64 - 24));        // 24 bits, ls byte
      sample_length = 4;
      break;
    case SPS_FORMAT_S16_LE:
      *op++ = (uint8_t)(hyper_sample >> (64 - 16));
      *op++ = (uint8_t)(hyper_sample >> (64 - 16 + 8)); // 16 bits, ms byte
      sample_length = 2;
      break;
    case SPS_FORMAT_S16_BE:
      *op++ = (uint8_t)(hyper_sample >> (64 - 16 + 8)); // 16 bits, ms byte
      *op = (uint8_t)(hyper_sample >> (64 - 16));
      sample_length = 2;
      break;
    case SPS_FORMAT_S16:
      *(int16_t *)op = (int16_t)(hyper_sample >> (64 - 16));
      sample_length = 2;
      break;
    case SPS_FORMAT_S8:
      *op = (int8_t)(hyper_sample >> (64 - 8));
      sample_length = 1;
      break;
    case SPS_FORMAT_U8:
      *op = 128 + (uint8_t)(hyper_sample >> (64 - 8));
      sample_length = 1;
      break;
    default:
      sample_length = 0; // stop a compiler warning
      die("Unexpected SPS_FORMAT_* with index %d while outputting silence",
          format);
    }
    p += sample_length;
    previous_random_number = r;
  }
  r64_unlock;
  return previous_random_number;
}

// This will check the incoming string "s" of length "len" with the existing
// NUL-terminated string "str" and update "flag" accordingly.

// Note: if the incoming string length is zero, then the a NULL is used; i.e. no
// zero-length strings are stored.

// If the strings are different, the str is free'd and replaced by a pointer
// to a newly strdup'd string and the flag is set
// If they are the same, the flag is cleared

int string_update_with_size(char **str, int *flag, char *s, size_t len) {
  if (*str) {
    if ((s) && (len)) {
      if ((len != strlen(*str)) || (strncmp(*str, s, len) != 0)) {
        free(*str);
        //*str = strndup(s, len); // it seems that OpenWrt 12 doesn't have this
        char *p = malloc(len + 1);
        memcpy(p, s, len);
        p[len] = '\0';
        *str = p;
        *flag = 1;
      } else {
        *flag = 0;
      }
    } else {
      // old string is non-NULL, new string is NULL or length 0
      free(*str);
      *str = NULL;
      *flag = 1;
    }
  } else { // old string is NULL
    if ((s) && (len)) {
      //*str = strndup(s, len); // it seems that OpenWrt 12 doesn't have this
      char *p = malloc(len + 1);
      memcpy(p, s, len);
      p[len] = '\0';
      *str = p;
      *flag = 1;
    } else {
      // old string is NULL and new string is NULL or length 0
      *flag = 0; // so no change
    }
  }
  return *flag;
}

// from https://stackoverflow.com/questions/13663617/memdup-function-in-c, with
// thanks
void *memdup(const void *mem, size_t size) {
  void *out = malloc(size);

  if (out != NULL)
    memcpy(out, mem, size);

  return out;
}

// This will allocate memory and place the NUL-terminated hex character
// equivalent of the bytearray passed in whose length is given.
char *debug_malloc_hex_cstring(void *packet, size_t nread) {
  char *response = malloc(nread * 3 + 1);
  unsigned char *q = packet;
  char *obfp = response;
  size_t obfc;
  for (obfc = 0; obfc < nread; obfc++) {
    snprintf(obfp, 4, "%02x ", *q);
    obfp += 3; // two digit characters and a space
    q++;
  };
  obfp--; // overwrite the last space with a NUL
  *obfp = 0;
  return response;
}

// the difference between two unsigned 32-bit modulo values as a signed 32-bit
// result now, if the two numbers are constrained to be within 2^(n-1)-1 of one
// another, we can use their as a signed 2^n bit number which will be positive
// if the first number is the same or "after" the second, and
// negative otherwise

int32_t mod32Difference(uint32_t a, uint32_t b) {
  int32_t result = a - b;
  return result;
}

int get_device_id(uint8_t *id, int int_length) {
  int response = 0;
  struct ifaddrs *ifaddr = NULL;
  struct ifaddrs *ifa = NULL;
  int i = 0;
  uint8_t *t = id;
  for (i = 0; i < int_length; i++) {
    *t++ = 0;
  }

  if (getifaddrs(&ifaddr) == -1) {
    debug(1, "getifaddrs");
    response = -1;
  } else {
    t = id;
    int found = 0;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {

      if ((ifa->ifa_addr) && (ifa->ifa_addr->sa_family == AF_PACKET)) {
        struct sockaddr_ll *s = (struct sockaddr_ll *)ifa->ifa_addr;
        if ((strcmp(ifa->ifa_name, "lo") != 0) && (found == 0)) {
          for (i = 0; ((i < s->sll_halen) && (i < int_length)); i++) {
            *t++ = s->sll_addr[i];
          }
          found = 1;
        }
      }
    }
    freeifaddrs(ifaddr);
  }
  return response;
}
