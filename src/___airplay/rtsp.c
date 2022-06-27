/*
 * RTSP protocol handler. This file is part of Shairport Sync
 * Copyright (c) James Laird 2013

 * Modifications associated with audio synchronization, mutithreading and
 * metadata handling copyright (c) Mike Brady 2014-2021
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

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <inttypes.h>
#include <limits.h>
#include <memory.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/ioctl.h>

#include "activity_monitor.h"
#include "config.h"

#include <openssl/md5.h>

#include "common.h"
#include "player.h"
#include "rtp.h"
#include "rtsp.h"

#ifdef AF_INET6
#define INETx_ADDRSTRLEN INET6_ADDRSTRLEN
#else
#define INETx_ADDRSTRLEN INET_ADDRSTRLEN
#endif

#include "pair.h"
#include "plist/plist.h"
#include "plist_get_info_response_xml.h"
#include "ptp-utilities.h"

#include "mdns.h"

// mDNS advertisement strings

// Create these strings and then keep them updated.
// When necessary, update the mDNS service records, using e.g. Avahi
// from these sources.

char *txt_records[64];
char *secondary_txt_records[64];

char firmware_version[64];
char ap1_featuresString[64];
char pkString[128];

char deviceIdString[64];
char featuresString[64];
char statusflagsString[32];
char piString[128];
char gidString[128];

#define METADATA_SNDBUF (4 * 1024 * 1024)

enum rtsp_read_request_response {
  rtsp_read_request_response_ok,
  rtsp_read_request_response_immediate_shutdown_requested,
  rtsp_read_request_response_bad_packet,
  rtsp_read_request_response_channel_closed,
  rtsp_read_request_response_read_error,
  rtsp_read_request_response_error
};

rtsp_conn_info *playing_conn;
rtsp_conn_info **conns;

int metadata_running = 0;

// always lock this when accessing the playing conn value
pthread_mutex_t playing_conn_lock = PTHREAD_MUTEX_INITIALIZER;

// always lock this when accessing the list of connection threads
pthread_mutex_t conns_lock = PTHREAD_MUTEX_INITIALIZER;

// every time we want to retain or release a reference count, lock it with this
// if a reference count is read as zero, it means the it's being deallocated.
static pthread_mutex_t reference_counter_lock = PTHREAD_MUTEX_INITIALIZER;

// only one thread is allowed to use the player at once.
// it monitors the request variable (at least when interrupted)
// static pthread_mutex_t playing_mutex = PTHREAD_MUTEX_INITIALIZER;
// static int please_shutdown = 0;
// static pthread_t playing_thread = 0;

int RTSP_connection_index = 1;
static int msg_indexes = 1;

typedef struct {
  int index_number;
  uint32_t referenceCount; // we might start using this...
  unsigned int nheaders;
  char *name[16];
  char *value[16];

  uint32_t contentlength;
  char *content;

  // for requests
  char method[16];
  char path[256];

  // for responses
  int respcode;
} rtsp_message;

int add_pstring_to_malloc(const char *s, void **allocation, size_t *size) {
  int response = 0;
  void *p = *allocation;
  if (p == NULL) {
    p = malloc(strlen(s) + 1);
    if (p == NULL) {
      debug(1, "error allocating memory");
    } else {
      *allocation = p;
      *size = *size + strlen(s) + 1;
      uint8_t *b = (uint8_t *)p;
      *b = strlen(s);
      p = p + 1;
      memcpy(p, s, strlen(s));
      response = 1;
    }
  } else {
    p = realloc(p, *size + strlen(s) + 1);
    if (p == NULL) { // assuming we never allocate a zero byte space
      debug(1, "error reallocating memory");
    } else {
      *allocation = p;
      uint8_t *b = (uint8_t *)p + *size;
      *b = strlen(s);
      p = p + *size + 1;
      memcpy(p, s, strlen(s));
      *size = *size + strlen(s) + 1;
      response = 1;
    }
  }
  return response;
}

static void pkString_make(char *str, size_t str_size, const char *device_id) {
  uint8_t public_key[32];
  if (str_size < 2 * sizeof(public_key) + 1) {
    warn("Insufficient string size");
    str[0] = '\0';
    return;
  }
  pair_public_key_get(PAIR_SERVER_HOMEKIT, public_key, device_id);
  char *ptr = str;
  for (size_t i = 0; i < sizeof(public_key); i++)
    ptr += sprintf(ptr, "%02x", public_key[i]);
}

void build_bonjour_strings(rtsp_conn_info *conn) {
  int entry_number = 0;

  // make up a firmware version
  if (git_version_string[0] != '\0')
    snprintf(firmware_version, sizeof(firmware_version), "fv=%s", git_version_string);
  else
    snprintf(firmware_version, sizeof(firmware_version), "fv=%s", PACKAGE_VERSION);

  uint64_t features_hi = config.airplay_features;
  features_hi = (features_hi >> 32) & 0xffffffff;
  uint64_t features_lo = config.airplay_features;
  features_lo = features_lo & 0xffffffff;
  snprintf(ap1_featuresString, sizeof(ap1_featuresString), "ft=0x%" PRIX64 ",0x%" PRIX64 "",
           features_lo, features_hi);
  snprintf(pkString, sizeof(pkString), "pk=");
  pkString_make(pkString + strlen("pk="), sizeof(pkString) - strlen("pk="),
                config.airplay_device_id);

  txt_records[entry_number++] = "cn=0,1";
  txt_records[entry_number++] = "da=true";
  txt_records[entry_number++] = "et=0,4";
  txt_records[entry_number++] = ap1_featuresString;
  txt_records[entry_number++] = firmware_version;
  txt_records[entry_number++] = "md=2";
  txt_records[entry_number++] = "am=Shairport Sync";
  txt_records[entry_number++] = "sf=0x4";
  txt_records[entry_number++] = "tp=UDP";
  txt_records[entry_number++] = "vn=65537";
  txt_records[entry_number++] = "vs=366.0";
  txt_records[entry_number++] = pkString;
  txt_records[entry_number++] = NULL;

  // make up a secondary set of text records
  entry_number = 0;

  secondary_txt_records[entry_number++] = "srcvers=366.0";
  snprintf(deviceIdString, sizeof(deviceIdString), "deviceid=%s", config.airplay_device_id);
  secondary_txt_records[entry_number++] = deviceIdString;
  snprintf(featuresString, sizeof(featuresString), "features=0x%" PRIX64 ",0x%" PRIX64 "",
           features_lo, features_hi);
  secondary_txt_records[entry_number++] = featuresString;
  snprintf(statusflagsString, sizeof(statusflagsString), "flags=0x%" PRIX32,
           config.airplay_statusflags);

  secondary_txt_records[entry_number++] = statusflagsString;
  secondary_txt_records[entry_number++] = "protovers=1.1";
  secondary_txt_records[entry_number++] = "acl=0";
  secondary_txt_records[entry_number++] = "rsf=0x0";
  secondary_txt_records[entry_number++] = firmware_version;
  secondary_txt_records[entry_number++] = "model=Shairport Sync";
  snprintf(piString, sizeof(piString), "pi=%s", config.airplay_pi);
  secondary_txt_records[entry_number++] = piString;
  if ((conn != NULL) && (conn->airplay_gid != 0)) {
    snprintf(gidString, sizeof(gidString), "gid=%s", conn->airplay_gid);
  } else {
    snprintf(gidString, sizeof(gidString), "gid=%s", config.airplay_pi);
  }
  secondary_txt_records[entry_number++] = gidString;
  if ((conn != NULL) && (conn->groupContainsGroupLeader != 0))
    secondary_txt_records[entry_number++] = "gcgl=1";
  else
    secondary_txt_records[entry_number++] = "gcgl=0";
  if ((conn != NULL) && (conn->airplay_gid != 0)) // if it's in a group
    secondary_txt_records[entry_number++] = "isGroupLeader=0";
  secondary_txt_records[entry_number++] = pkString;
  secondary_txt_records[entry_number++] = NULL;
}

int have_play_lock(rtsp_conn_info *conn) {
  int response = 0;
  debug_mutex_lock(&playing_conn_lock, 1000000, 3);
  if (playing_conn == conn) // this connection definitely has the play lock
    response = 1;
  debug_mutex_unlock(&playing_conn_lock, 3);
  return response;
}

// return 0 if the playing_conn isn't already locked by someone else and if
// it belongs to the conn passed in.
// remember to release it!
int try_to_hold_play_lock(rtsp_conn_info *conn) {
  int response = -1;
  if (pthread_mutex_trylock(&playing_conn_lock) == 0) {
    if (playing_conn == conn) {
      response = 0;
    } else {
      pthread_mutex_unlock(&playing_conn_lock);
    }
  }
  return response;
}

void release_hold_on_play_lock(__attribute__((unused)) rtsp_conn_info *conn) {
  pthread_mutex_unlock(&playing_conn_lock);
}

void release_play_lock(rtsp_conn_info *conn) {
  debug(2, "Connection %d: release play lock.", conn->connection_number);
  debug_mutex_lock(&playing_conn_lock, 1000000, 3);
  if (playing_conn == conn) { // if we have the player
    playing_conn = NULL;      // let it go
    debug(2, "Connection %d: release play lock.", conn->connection_number);
  }
  debug_mutex_unlock(&playing_conn_lock, 3);
}

int get_play_lock(rtsp_conn_info *conn) {
  debug(2, "Connection %d: request play lock.", conn->connection_number);
  // returns -1 if it failed, 0 if it succeeded and 1 if it succeeded but
  // interrupted an existing session
  int response = 0;

  int have_the_player = 0;
  int should_wait = 0; // this will be true if you're trying to break in to the
                       // current session
  int interrupting_current_session = 0;

  // try to become the current playing_conn

  debug_mutex_lock(&playing_conn_lock, 1000000, 3); // get it

  if (playing_conn == NULL) {
    playing_conn = conn;
    have_the_player = 1;
  } else if (playing_conn == conn) {
    have_the_player = 1;
    warn("Duplicate attempt to acquire the player by the same connection, by "
         "the look of it!");
  } else if (playing_conn->stop) {
    debug(1, "Connection %d: Waiting for Connection %d to stop playing.", conn->connection_number,
          playing_conn->connection_number);
    should_wait = 1;
  } else { // ignore the allow_session_interruption in AirPlay 2 -- it is always
           // permissible, it
    // seems

    debug(2, "Connection %d: Asking Connection %d to stop playing.", conn->connection_number,
          playing_conn->connection_number);
    playing_conn->stop = 1;
    interrupting_current_session = 1;
    should_wait = 1;
    pthread_cancel(playing_conn->thread); // asking the RTSP thread to exit
  }
  debug_mutex_unlock(&playing_conn_lock, 3);

  if (should_wait) {
    int time_remaining = 3000000; // must be signed, as it could go negative...

    while ((time_remaining > 0) && (have_the_player == 0)) {
      debug_mutex_lock(&playing_conn_lock, 1000000, 3); // get it
      if (playing_conn == NULL) {
        playing_conn = conn;
        have_the_player = 1;
      }
      debug_mutex_unlock(&playing_conn_lock, 3);

      if (have_the_player == 0) {
        usleep(100000);
        time_remaining -= 100000;
      }
    }

    if ((have_the_player == 1) && (interrupting_current_session == 1)) {
      debug(2, "Connection %d: Got player lock", conn->connection_number);
      response = 1;
    } else {
      debug(1, "Connection %d: failed to get player lock after waiting.", conn->connection_number);
      response = -1;
    }
  }

  if ((have_the_player == 1) && (interrupting_current_session == 0)) {
    debug(2, "Connection %d: Got player lock.", conn->connection_number);
    response = 0;
  }
  return response;
}

void player_watchdog_thread_cleanup_handler(void *arg) {
  rtsp_conn_info *conn = (rtsp_conn_info *)arg;
  debug(3, "Connection %d: Watchdog Exit.", conn->connection_number);
}

void *player_watchdog_thread_code(void *arg) {
  pthread_cleanup_push(player_watchdog_thread_cleanup_handler, arg);
  rtsp_conn_info *conn = (rtsp_conn_info *)arg;
  do {
    usleep(2000000); // check every two seconds
                     // debug(3, "Connection %d: Check the thread is doing
                     // something...", conn->connection_number);

    if ((config.dont_check_timeout == 0) && (config.timeout != 0) &&
        (conn->airplay_type == ap_1)) {
      debug_mutex_lock(&conn->watchdog_mutex, 1000, 0);
      uint64_t last_watchdog_bark_time = conn->watchdog_bark_time;
      debug_mutex_unlock(&conn->watchdog_mutex, 0);
      if (last_watchdog_bark_time != 0) {
        uint64_t time_since_last_bark =
            (get_absolute_time_in_ns() - last_watchdog_bark_time) / 1000000000;
        uint64_t ct = config.timeout; // go from int to 64-bit int

        if (time_since_last_bark >= ct) {
          conn->watchdog_barks++;
          if (conn->watchdog_barks == 1) {
            // debuglev = 3; // tell us everything.
            debug(1,
                  "Connection %d: As Yeats almost said, \"Too long a silence / "
                  "can make a stone "
                  "of the heart\".",
                  conn->connection_number);
            conn->stop = 1;
            pthread_cancel(conn->thread);
          } else if (conn->watchdog_barks == 3) {
            if ((conn->unfixable_error_reported == 0)) {
              conn->unfixable_error_reported = 1;
            } else {
              die("an unrecoverable error, \"unable_to_cancel_play_session\", "
                  "has been detected.",
                  conn->connection_number);
            }
          }
        }
      }
    }
  } while (1);
  pthread_cleanup_pop(0); // should never happen
  pthread_exit(NULL);
}

// keep track of the threads we have spawned so we can join() them
static int nconns = 0;
static void track_thread(rtsp_conn_info *conn) {
  debug_mutex_lock(&conns_lock, 1000000, 3);
  // look for an empty slot first
  int i = 0;
  int found = 0;
  while ((i < nconns) && (found == 0)) {
    if (conns[i] == NULL)
      found = 1;
    else
      i++;
  }
  if (found != 0) {
    conns[i] = conn;
  } else {
    // make space for a new element
    conns = realloc(conns, sizeof(rtsp_conn_info *) * (nconns + 1));
    if (conns) {
      conns[nconns] = conn;
      nconns++;
    } else {
      die("could not reallocate memory for conns");
    }
  }
  debug_mutex_unlock(&conns_lock, 3);
}

// note: connection numbers start at 1, so an except_this_one value of zero
// means "all threads"
void cancel_all_RTSP_threads(airplay_stream_c stream_category, int except_this_one) {
  // if the stream category is unspecified_stream_category
  // all categories are elegible for cancellation
  // otherwise just the category itself
  debug_mutex_lock(&conns_lock, 1000000, 3);
  int i;
  for (i = 0; i < nconns; i++) {
    if ((conns[i] != NULL) && (conns[i]->running != 0) &&
        (conns[i]->connection_number != except_this_one) &&
        ((stream_category == unspecified_stream_category) ||
         (stream_category == conns[i]->airplay_stream_category))) {
      pthread_cancel(conns[i]->thread);
      debug(2, "Connection %d: cancelled.", conns[i]->connection_number);
    }
  }
  for (i = 0; i < nconns; i++) {
    if ((conns[i] != NULL) && (conns[i]->running != 0) &&
        (conns[i]->connection_number != except_this_one) &&
        ((stream_category == unspecified_stream_category) ||
         (stream_category == conns[i]->airplay_stream_category))) {
      pthread_join(conns[i]->thread, NULL);
      debug(2, "Connection %d: joined.", conns[i]->connection_number);
      free(conns[i]);
      conns[i] = NULL;
    }
  }
  debug_mutex_unlock(&conns_lock, 3);
}

void cleanup_threads(void) {
  void *retval;
  int i;
  // debug(2, "culling threads.");
  debug_mutex_lock(&conns_lock, 1000000, 3);
  for (i = 0; i < nconns; i++) {
    if ((conns[i] != NULL) && (conns[i]->running == 0)) {
      debug(3, "found RTSP connection thread %d in a non-running state.",
            conns[i]->connection_number);
      pthread_join(conns[i]->thread, &retval);
      debug(2, "Connection %d: deleted in cleanup.", conns[i]->connection_number);
      free(conns[i]);
      conns[i] = NULL;
    }
  }
  debug_mutex_unlock(&conns_lock, 3);
}

// park a null at the line ending, and return the next line pointer
// accept \r, \n, or \r\n
static char *nextline(char *in, int inbuf) {
  char *out = NULL;
  while (inbuf) {
    if (*in == '\r') {
      *in++ = 0;
      out = in;
      inbuf--;
    }
    if ((*in == '\n') && (inbuf)) {
      *in++ = 0;
      out = in;
    }

    if (out)
      break;

    in++;
    inbuf--;
  }
  return out;
}

void msg_retain(rtsp_message *msg) {
  int rc = pthread_mutex_lock(&reference_counter_lock);
  if (rc)
    debug(1, "Error %d locking reference counter lock");
  if (msg > (rtsp_message *)0x00010000) {
    msg->referenceCount++;
    debug(3, "msg_free increment reference counter message %d to %d.", msg->index_number,
          msg->referenceCount);
    // debug(1,"msg_retain -- item %d reference count %d.", msg->index_number,
    // msg->referenceCount);
    rc = pthread_mutex_unlock(&reference_counter_lock);
    if (rc)
      debug(1, "Error %d unlocking reference counter lock");
  } else {
    debug(1, "invalid rtsp_message pointer 0x%x passed to retain", (uintptr_t)msg);
  }
}

rtsp_message *msg_init(void) {
  rtsp_message *msg = malloc(sizeof(rtsp_message));
  if (msg) {
    memset(msg, 0, sizeof(rtsp_message));
    msg->referenceCount = 1; // from now on, any access to this must be protected with the lock
    msg->index_number = msg_indexes++;
    debug(3, "msg_init message %d", msg->index_number);
  } else {
    die("msg_init -- can not allocate memory for rtsp_message %d.", msg_indexes);
  }
  // debug(1,"msg_init -- create item %d.", msg->index_number);
  return msg;
}

int msg_add_header(rtsp_message *msg, char *name, char *value) {
  if (msg->nheaders >= sizeof(msg->name) / sizeof(char *)) {
    warn("too many headers?!");
    return 1;
  }

  msg->name[msg->nheaders] = strdup(name);
  msg->value[msg->nheaders] = strdup(value);
  msg->nheaders++;

  return 0;
}

char *msg_get_header(rtsp_message *msg, char *name) {
  unsigned int i;
  for (i = 0; i < msg->nheaders; i++)
    if (!strcasecmp(msg->name[i], name))
      return msg->value[i];
  return NULL;
}

void debug_print_msg_headers(int level, rtsp_message *msg) {
  unsigned int i;
  for (i = 0; i < msg->nheaders; i++) {
    debug(level, "  Type: \"%s\", content: \"%s\"", msg->name[i], msg->value[i]);
  }
}

void msg_free(rtsp_message **msgh) {
  debug_mutex_lock(&reference_counter_lock, 1000, 0);
  if (*msgh > (rtsp_message *)0x00010000) {
    rtsp_message *msg = *msgh;
    msg->referenceCount--;
    if (msg->referenceCount)
      debug(3, "msg_free decrement reference counter message %d to %d", msg->index_number,
            msg->referenceCount);
    if (msg->referenceCount == 0) {
      unsigned int i;
      for (i = 0; i < msg->nheaders; i++) {
        free(msg->name[i]);
        free(msg->value[i]);
      }
      if (msg->content)
        free(msg->content);
      // debug(1,"msg_free item %d -- free.",msg->index_number);
      uintptr_t index = (msg->index_number) & 0xFFFF;
      if (index == 0)
        index = 0x10000;               // ensure it doesn't fold to zero.
      *msgh = (rtsp_message *)(index); // put a version of the index number of
                                       // the freed message in here
      debug(3, "msg_free freed message %d", msg->index_number);
      free(msg);
    } else {
      // debug(1,"msg_free item %d -- decrement reference to
      // %d.",msg->index_number,msg->referenceCount);
    }
  } else if (*msgh != NULL) {
    debug(1,
          "msg_free: error attempting to free an allocated but already-freed "
          "rtsp_message, number "
          "%d.",
          (uintptr_t)*msgh);
  }
  debug_mutex_unlock(&reference_counter_lock, 0);
}

int msg_handle_line(rtsp_message **pmsg, char *line) {
  rtsp_message *msg = *pmsg;

  if (!msg) {
    msg = msg_init();
    *pmsg = msg;
    char *sp, *p;
    sp = NULL; // this is to quieten a compiler warning

    debug(3, "RTSP Message Received: \"%s\".", line);

    p = strtok_r(line, " ", &sp);
    if (!p)
      goto fail;
    strncpy(msg->method, p, sizeof(msg->method) - 1);

    p = strtok_r(NULL, " ", &sp);
    if (!p)
      goto fail;
    strncpy(msg->path, p, sizeof(msg->path) - 1);

    p = strtok_r(NULL, " ", &sp);
    if (!p)
      goto fail;
    if (strcmp(p, "RTSP/1.0"))
      goto fail;

    return -1;
  }

  if (strlen(line)) {
    char *p;
    p = strstr(line, ": ");
    if (!p) {
      warn("bad header: >>%s<<", line);
      goto fail;
    }
    *p = 0;
    p += 2;
    msg_add_header(msg, line, p);
    debug(3, "    %s: %s.", line, p);
    return -1;
  } else {
    char *cl = msg_get_header(msg, "Content-Length");
    if (cl)
      return atoi(cl);
    else
      return 0;
  }

fail:
  debug(3, "msg_handle_line fail");
  msg_free(pmsg);
  *pmsg = NULL;
  return 0;
}

void add_flush_request(int flushNow, uint32_t flushFromSeq, uint32_t flushFromTS,
                       uint32_t flushUntilSeq, uint32_t flushUntilTS, rtsp_conn_info *conn) {
  // immediate flush requests are added sequentially. Don't know how more than
  // one could arise, TBH
  flush_request_t **t = &conn->flush_requests;
  int done = 0;
  do {
    flush_request_t *u = *t;
    if ((u == NULL) || ((u->flushNow == 0) && (flushNow != 0)) ||
        (flushFromSeq < u->flushFromSeq) ||
        ((flushFromSeq == u->flushFromSeq) && (flushFromTS < u->flushFromTS))) {
      flush_request_t *n = (flush_request_t *)calloc(sizeof(flush_request_t), 1);
      n->flushNow = flushNow;
      n->flushFromSeq = flushFromSeq;
      n->flushFromTS = flushFromTS;
      n->flushUntilSeq = flushUntilSeq;
      n->flushUntilTS = flushUntilTS;
      n->next = u;
      *t = n;
      done = 1;
    } else {
      t = &u->next;
    }
  } while (done == 0);
}

void display_all_flush_requests(rtsp_conn_info *conn) {
  if (conn->flush_requests == NULL) {
    debug(1, "No flush requests.");
  } else {
    flush_request_t *t = conn->flush_requests;
    do {
      if (t->flushNow) {
        debug(1, "immediate flush          to untilSeq: %u, untilTS: %u.", t->flushUntilSeq,
              t->flushUntilTS);
      } else {
        debug(1, "fromSeq: %u, fromTS: %u, to untilSeq: %u, untilTS: %u.", t->flushFromSeq,
              t->flushFromTS, t->flushUntilSeq, t->flushUntilTS);
      }
      t = t->next;
    } while (t != NULL);
  }
}

int rtsp_message_contains_plist(rtsp_message *message) {
  int reply = 0; // assume there is no plist in the message
  if ((message->contentlength >= strlen("bplist00")) &&
      (strstr(message->content, "bplist00") == message->content))
    reply = 1;
  return reply;
}

plist_t plist_from_rtsp_content(rtsp_message *message) {
  plist_t the_plist = NULL;
  if (rtsp_message_contains_plist(message)) {
    plist_from_memory(message->content, message->contentlength, &the_plist);
  }
  return the_plist;
}

char *plist_content(plist_t the_plist) {
  // caller must free the returned character buffer
  // convert it to xml format
  uint32_t size;
  char *plist_out = NULL;
  plist_to_xml(the_plist, &plist_out, &size);

  // put it into a NUL-terminated string
  char *reply = malloc(size + 1);
  if (reply) {
    memcpy(reply, plist_out, size);
    reply[size] = '\0';
  }
  if (the_plist)
    plist_free(the_plist);
  if (plist_out)
    free(plist_out);
  return reply;
}

// caller must free the returned character buffer
char *rtsp_plist_content(rtsp_message *message) {
  char *reply = NULL;
  // first, check if it has binary plist content
  if (rtsp_message_contains_plist(message)) {
    // get the plist from the content

    plist_t the_plist = plist_from_rtsp_content(message);

    // convert it to xml format
    uint32_t size;
    char *plist_out = NULL;
    plist_to_xml(the_plist, &plist_out, &size);

    // put it into a NUL-terminated string
    reply = malloc(size + 1);
    if (reply) {
      memcpy(reply, plist_out, size);
      reply[size] = '\0';
    }
    if (the_plist)
      plist_free(the_plist);
    if (plist_out)
      free(plist_out);
  }
  return reply;
}

void debug_log_rtsp_message(int level, char *prompt, rtsp_message *message) {
  if (level > debuglev)
    return;
  if ((prompt) && (*prompt != '\0')) // okay to pass NULL or an empty list...
    debug(level, prompt);
  // debug_print_msg_headers(level, message);

  char *plist_content = rtsp_plist_content(message);
  if (plist_content) {
    debug(level + 1, "  Content Plist (as XML):\n--\n%s--", plist_content);
    free(plist_content);
  } else

  {
    debug(level, "  No Content Plist. Content length: %d.", message->contentlength);
  }
}

static void buf_add(ap2_buffer *buf, uint8_t *in, size_t in_len) {
  if (buf->len + in_len > buf->size) {
    buf->size = buf->len + in_len + 2048; // Extra legroom to avoid future memcpy's
    uint8_t *new = malloc(buf->size);
    memcpy(new, buf->data, buf->len);
    free(buf->data);
    buf->data = new;
  }
  memcpy(buf->data + buf->len, in, in_len);
  buf->len += in_len;
}

static void buf_drain(ap2_buffer *buf, ssize_t len) {
  if (len < 0 || (size_t)len >= buf->len) {
    free(buf->data);
    memset(buf, 0, sizeof(ap2_buffer));
    return;
  }
  memmove(buf->data, buf->data + len, buf->len - len);
  buf->len -= len;
}

static size_t buf_remove(ap2_buffer *buf, uint8_t *out, size_t out_len) {
  size_t bytes = (buf->len > out_len) ? out_len : buf->len;
  memcpy(out, buf->data, bytes);
  buf_drain(buf, bytes);
  return bytes;
}

static ssize_t read_encrypted(int fd, ap2_pairing *ctx, void *buf, size_t count) {
  uint8_t in[4096];
  uint8_t *plain;
  size_t plain_len;

  // If there is leftover decoded content from the last pass just return that
  if (ctx->plain_buf.len > 0) {
    return buf_remove(&ctx->plain_buf, buf, count);
  }

  do {
    ssize_t got = read(fd, in, sizeof(in));
    if (got <= 0)
      return got;
    buf_add(&ctx->encrypted_buf, in, got);

    ssize_t consumed = pair_decrypt(&plain, &plain_len, ctx->encrypted_buf.data,
                                    ctx->encrypted_buf.len, ctx->cipher_ctx);
    if (consumed < 0)
      return -1;
    buf_drain(&ctx->encrypted_buf, consumed);
  } while (plain_len == 0);

  // Fast path, avoids some memcpy + allocs in case of the normal, small message
  /*  if (ctx->plain_buf.len == 0 && plain_len < count) {
      memcpy(buf, plain, plain_len);
      free(plain);
      return plain_len;
    }
  */
  buf_add(&ctx->plain_buf, plain, plain_len);
  free(plain);

  return buf_remove(&ctx->plain_buf, buf, count);
}

static ssize_t write_encrypted(rtsp_conn_info *conn, const void *buf, size_t count) {
  uint8_t *encrypted;
  size_t encrypted_len;

  ssize_t ret =
      pair_encrypt(&encrypted, &encrypted_len, buf, count, conn->ap2_control_pairing.cipher_ctx);
  if (ret < 0) {
    debug(1, pair_cipher_errmsg(conn->ap2_control_pairing.cipher_ctx));
    return -1;
  }

  size_t remain = encrypted_len;
  while (remain > 0) {
    ssize_t wrote = write(conn->fd, encrypted + (encrypted_len - remain), remain);
    if (wrote <= 0) {
      free(encrypted);
      return wrote;
    }
    remain -= wrote;
  }
  free(encrypted);
  return count;
}

ssize_t read_from_rtsp_connection(rtsp_conn_info *conn, void *buf, size_t count) {
  if (conn->ap2_control_pairing.cipher_ctx) {
    conn->ap2_control_pairing.is_encrypted = 1;
    return read_encrypted(conn->fd, &conn->ap2_control_pairing, buf, count);
  } else {
    return read(conn->fd, buf, count);
  }
}

enum rtsp_read_request_response rtsp_read_request(rtsp_conn_info *conn,
                                                  rtsp_message **the_packet) {
  *the_packet = NULL; // need this for error handling

  enum rtsp_read_request_response reply = rtsp_read_request_response_ok;
  ssize_t buflen = 4096;

  int release_buffer = 0;         // on exit, don't deallocate the buffer if everything was okay
  char *buf = malloc(buflen + 1); // add a NUL at the end
  if (!buf) {
    warn("Connection %d: rtsp_read_request: can't get a buffer.", conn->connection_number);
    return (rtsp_read_request_response_error);
  }
  pthread_cleanup_push(malloc_cleanup, buf);
  ssize_t nread;
  ssize_t inbuf = 0;
  int msg_size = -1;

  while (msg_size < 0) {
    if (conn->stop != 0) {
      debug(3, "Connection %d: shutdown requested.", conn->connection_number);
      reply = rtsp_read_request_response_immediate_shutdown_requested;
      goto shutdown;
    }

    nread = read_from_rtsp_connection(conn, buf + inbuf, buflen - inbuf);

    if (nread == 0) {
      // a blocking read that returns zero means eof -- implies connection
      // closed
      debug(3, "Connection %d: -- connection closed.", conn->connection_number);
      reply = rtsp_read_request_response_channel_closed;
      goto shutdown;
    }

    if (nread < 0) {
      if (errno == EINTR)
        continue;
      if (errno == EAGAIN) {
        debug(1, "Connection %d: getting Error 11 -- EAGAIN from a blocking read!",
              conn->connection_number);
        continue;
      }
      if (errno != ECONNRESET) {
        char errorstring[1024];
        strerror_r(errno, (char *)errorstring, sizeof(errorstring));
        if (errno != 0)
          debug(2,
                "Connection %d: rtsp_read_request_response_read_error %d: "
                "\"%s\".",
                conn->connection_number, errno, (char *)errorstring);
      }
      reply = rtsp_read_request_response_read_error;
      goto shutdown;
    }

    inbuf += nread;

    char *next;
    while (msg_size < 0 && (next = nextline(buf, inbuf))) {
      msg_size = msg_handle_line(the_packet, buf);

      if (!(*the_packet)) {
        debug(1, "Connection %d: rtsp_read_request can't find an RTSP header.",
              conn->connection_number);
        reply = rtsp_read_request_response_bad_packet;
        goto shutdown;
      }

      inbuf -= next - buf;
      if (inbuf)
        memmove(buf, next, inbuf);
    }
  }

  if (msg_size > buflen) {
    buf = realloc(buf, msg_size + 1);
    if (!buf) {
      warn("Connection %d: too much content.", conn->connection_number);
      reply = rtsp_read_request_response_error;
      goto shutdown;
    }
    buflen = msg_size;
  }

  uint64_t threshold_time =
      get_absolute_time_in_ns() + ((uint64_t)15000000000); // i.e. fifteen seconds from now
  int warning_message_sent = 0;

  // const size_t max_read_chunk = 1024 * 1024 / 16;
  while (inbuf < msg_size) {
    // we are going to read the stream in chunks and time how long it takes to
    // do so.
    // If it's taking too long, (and we find out about it), we will send an
    // error message as
    // metadata

    if (warning_message_sent == 0) {
      uint64_t time_now = get_absolute_time_in_ns();
      if (time_now > threshold_time) { // it's taking too long
        debug(1, "Error receiving metadata from source -- transmission seems "
                 "to be stalled.");

        warning_message_sent = 1;
      }
    }

    if (conn->stop != 0) {
      debug(1, "RTSP shutdown requested.");
      reply = rtsp_read_request_response_immediate_shutdown_requested;
      goto shutdown;
    }
    size_t read_chunk = msg_size - inbuf;
    // if (read_chunk > max_read_chunk)
    //  read_chunk = max_read_chunk;
    // usleep(80000); // wait about 80 milliseconds between reads of up to
    // max_read_chunk
    nread = read_from_rtsp_connection(conn, buf + inbuf, read_chunk);
    if (!nread) {
      reply = rtsp_read_request_response_error;
      goto shutdown;
    }
    if (nread < 0) {
      if (errno == EINTR)
        continue;
      if (errno == EAGAIN) {
        debug(1, "Getting Error 11 -- EAGAIN from a blocking read!");
        continue;
      }
      if (errno != ECONNRESET) {
        char errorstring[1024];
        strerror_r(errno, (char *)errorstring, sizeof(errorstring));
        debug(1, "Connection %d: rtsp_read_request_response_read_error %d: \"%s\".",
              conn->connection_number, errno, (char *)errorstring);
      }
      reply = rtsp_read_request_response_read_error;
      goto shutdown;
    }
    inbuf += nread;
  }

  rtsp_message *msg = *the_packet;
  msg->contentlength = inbuf;
  msg->content = buf;
  char *jp = inbuf + buf;
  *jp = '\0';
  *the_packet = msg;
shutdown:
  if (reply != rtsp_read_request_response_ok) {
    msg_free(the_packet);
    release_buffer = 1; // allow the buffer to be released
  }
  pthread_cleanup_pop(release_buffer);
  return reply;
}

int msg_write_response(rtsp_conn_info *conn, rtsp_message *resp) {
  char pkt[4096];
  int pktfree = sizeof(pkt);
  char *p = pkt;
  int n;
  unsigned int i;

  struct response_t {
    int code;
    char *string;
  };

  struct response_t responses[] = {{200, "OK"},
                                   {400, "Bad Request"},
                                   {403, "Unauthorized"},
                                   {451, "Unavailable"},
                                   {501, "Not Implemented"}};
  // 451 is really "Unavailable For Legal Reasons"!
  int found = 0;
  char *respcode_text = "Unauthorized";
  for (i = 0; i < sizeof(responses) / sizeof(struct response_t); i++) {
    if (resp->respcode == responses[i].code) {
      found = 1;
      respcode_text = responses[i].string;
    }
  }

  if (found == 0)
    debug(1, "can't find text for response code %d. Using \"%s\" instead.", resp->respcode,
          respcode_text);

  n = snprintf(p, pktfree, "RTSP/1.0 %d %s\r\n", resp->respcode, respcode_text);
  pktfree -= n;
  p += n;

  for (i = 0; i < resp->nheaders; i++) {
    //    debug(3, "    %s: %s.", resp->name[i], resp->value[i]);
    n = snprintf(p, pktfree, "%s: %s\r\n", resp->name[i], resp->value[i]);
    pktfree -= n;
    p += n;
    if (pktfree <= 1024) {
      debug(1, "Attempted to write overlong RTSP packet 1");
      return -1;
    }
  }

  // Here, if there's content, write the Content-Length header ...

  if (resp->contentlength) {
    debug(2, "Responding with content of length %d", resp->contentlength);
    n = snprintf(p, pktfree, "Content-Length: %d\r\n", resp->contentlength);
    pktfree -= n;
    p += n;
    if (pktfree <= 1024) {
      debug(1, "Attempted to write overlong RTSP packet 2");
      return -2;
    }
  }

  n = snprintf(p, pktfree, "\r\n");
  pktfree -= n;
  p += n;

  if (resp->contentlength) {
    memcpy(p, resp->content, resp->contentlength);
    pktfree -= resp->contentlength;
    p += resp->contentlength;
  }

  if (pktfree <= 1024) {
    debug(1, "Attempted to write overlong RTSP packet 3");
    return -3;
  }

  // here, if the link is encrypted, better do it

  ssize_t reply;
  if (conn->ap2_control_pairing.is_encrypted) {
    reply = write_encrypted(conn, pkt, p - pkt);
  } else {
    reply = write(conn->fd, pkt, p - pkt);
  }

  if (reply == -1) {
    char errorstring[1024];
    strerror_r(errno, (char *)errorstring, sizeof(errorstring));
    debug(1, "msg_write_response error %d: \"%s\".", errno, (char *)errorstring);
    return -4;
  }
  if (reply != p - pkt) {
    debug(1,
          "msg_write_response error -- requested bytes: %d not fully written: "
          "%d.",
          p - pkt, reply);
    return -5;
  }
  return 0;
}

char *get_category_string(airplay_stream_c cat) {
  char *category;
  switch (cat) {
    case unspecified_stream_category:
      category = "unspecified stream";
      break;
    case ptp_stream:
      category = "PTP stream";
      break;
    case ntp_stream:
      category = "NTP stream";
      break;
    case remote_control_stream:
      category = "Remote Control stream";
      break;
    default:
      category = "Unexpected stream code";
      break;
  }
  return category;
}

void handle_record_2(rtsp_conn_info *conn, __attribute((unused)) rtsp_message *req,
                     rtsp_message *resp) {
  resp->respcode = 200;
}

void handle_get_info(__attribute((unused)) rtsp_conn_info *conn, rtsp_message *req,
                     rtsp_message *resp) {
  debug_log_rtsp_message(2, "GET /info:", req);
  if (rtsp_message_contains_plist(req)) { // it's stage one
    // get version of AirPlay -- it might be too old. Not using it yet.
    char *hdr = msg_get_header(req, "User-Agent");
    if (hdr) {
      if (strstr(hdr, "AirPlay/") == hdr) {
        hdr = hdr + strlen("AirPlay/");
        // double airplay_version = 0.0;
        // airplay_version = atof(hdr);
        debug(2, "Connection %d: GET_INFO: Source AirPlay Version is: %s.",
              conn->connection_number, hdr);
      }
    }
    plist_t info_plist = NULL;
    plist_from_memory(req->content, req->contentlength, &info_plist);

    plist_t qualifier = plist_dict_get_item(info_plist, "qualifier");
    if (qualifier == NULL) {
      debug(1, "GET /info Stage 1: plist->qualifier was NULL");
      goto user_fail;
    }
    if (plist_array_get_size(qualifier) < 1) {
      debug(1, "GET /info Stage 1: plist->qualifier array length < 1");
      goto user_fail;
    }
    plist_t qualifier_array_value = plist_array_get_item(qualifier, 0);
    char *qualifier_array_val_cstr;
    plist_get_string_val(qualifier_array_value, &qualifier_array_val_cstr);
    if (qualifier_array_val_cstr == NULL) {
      debug(1, "GET /info Stage 1: first item in qualifier array not a string");
      goto user_fail;
    }
    debug(2, "GET /info Stage 1: qualifier: %s", qualifier_array_val_cstr);
    plist_free(info_plist);
    free(qualifier_array_val_cstr);

    // uint8_t bt_addr[6] = {0xB8, 0x27, 0xEB, 0xB7, 0xD4, 0x0E};
    plist_t response_plist = NULL;
    plist_from_xml((const char *)plist_get_info_response_xml_data,
                   plist_get_info_response_xml_size, &response_plist);
    if (response_plist == NULL) {
      debug(1, "GET /info Stage 1: response plist not created from XML!");
    } else {
      void *qualifier_response_data = NULL;
      size_t qualifier_response_data_length = 0;
      if (add_pstring_to_malloc("acl=0", &qualifier_response_data,
                                &qualifier_response_data_length) == 0)
        debug(1, "Problem");
      if (add_pstring_to_malloc(deviceIdString, &qualifier_response_data,
                                &qualifier_response_data_length) == 0)
        debug(1, "Problem");
      if (add_pstring_to_malloc(featuresString, &qualifier_response_data,
                                &qualifier_response_data_length) == 0)
        debug(1, "Problem");
      if (add_pstring_to_malloc("rsf=0x0", &qualifier_response_data,
                                &qualifier_response_data_length) == 0)
        debug(1, "Problem");
      if (add_pstring_to_malloc("flags=0x4", &qualifier_response_data,
                                &qualifier_response_data_length) == 0)
        debug(1, "Problem");
      if (add_pstring_to_malloc("model=Shairport Sync", &qualifier_response_data,
                                &qualifier_response_data_length) == 0)
        debug(1, "Problem");
      if (add_pstring_to_malloc("manufacturer=", &qualifier_response_data,
                                &qualifier_response_data_length) == 0)
        debug(1, "Problem");
      if (add_pstring_to_malloc("serialNumber=", &qualifier_response_data,
                                &qualifier_response_data_length) == 0)
        debug(1, "Problem");
      if (add_pstring_to_malloc("protovers=1.1", &qualifier_response_data,
                                &qualifier_response_data_length) == 0)
        debug(1, "Problem");
      if (add_pstring_to_malloc("srcvers=366.0", &qualifier_response_data,
                                &qualifier_response_data_length) == 0)
        debug(1, "Problem");
      if (add_pstring_to_malloc(piString, &qualifier_response_data,
                                &qualifier_response_data_length) == 0)
        debug(1, "Problem");
      if (add_pstring_to_malloc(gidString, &qualifier_response_data,
                                &qualifier_response_data_length) == 0)
        debug(1, "Problem");
      if (add_pstring_to_malloc("gcgl=0", &qualifier_response_data,
                                &qualifier_response_data_length) == 0)
        debug(1, "Problem");
      snprintf(pkString, sizeof(pkString), "pk=");
      pkString_make(pkString + strlen("pk="), sizeof(pkString) - strlen("pk="),
                    config.airplay_device_id);
      if (add_pstring_to_malloc(pkString, &qualifier_response_data,
                                &qualifier_response_data_length) == 0)
        debug(1, "Problem");
      // debug(1,"qualifier_response_data_length: %u.",
      // qualifier_response_data_length);

      plist_dict_set_item(response_plist, "txtAirPlay",
                          plist_new_data(qualifier_response_data, qualifier_response_data_length));

      plist_dict_set_item(response_plist, "features", plist_new_uint(config.airplay_features));
      plist_dict_set_item(response_plist, "statusFlags",
                          plist_new_uint(config.airplay_statusflags));
      plist_dict_set_item(response_plist, "deviceID", plist_new_string(config.airplay_device_id));
      plist_dict_set_item(response_plist, "pi", plist_new_string(config.airplay_pi));
      plist_dict_set_item(response_plist, "name", plist_new_string(config.service_name));
      char *vs = get_version_string();
      //      plist_dict_set_item(response_plist, "model",
      //      plist_new_string(vs));
      plist_dict_set_item(response_plist, "model", plist_new_string("Shairport Sync"));
      free(vs);
      // pkString_make(pkString, sizeof(pkString), config.airplay_device_id);
      // plist_dict_set_item(response_plist, "pk", plist_new_string(pkString));
      plist_to_bin(response_plist, &resp->content, &resp->contentlength);
      if (resp->contentlength == 0)
        debug(1, "GET /info Stage 1: response bplist not created!");
      plist_free(response_plist);
      free(qualifier_response_data);
    }
    msg_add_header(resp, "Content-Type", "application/x-apple-binary-plist");
    debug_log_rtsp_message(3, "GET /info Stage 1 Response:", resp);
    resp->respcode = 200;
    return;

  user_fail:
    resp->respcode = 400;
    return;
  } else { // stage two
    plist_t response_plist = NULL;
    plist_from_xml((const char *)plist_get_info_response_xml_data,
                   plist_get_info_response_xml_size, &response_plist);
    plist_dict_set_item(response_plist, "features", plist_new_uint(config.airplay_features));
    plist_dict_set_item(response_plist, "statusFlags", plist_new_uint(config.airplay_statusflags));
    plist_dict_set_item(response_plist, "deviceID", plist_new_string(config.airplay_device_id));
    plist_dict_set_item(response_plist, "pi", plist_new_string(config.airplay_pi));
    plist_dict_set_item(response_plist, "name", plist_new_string(config.service_name));
    char *vs = get_version_string();
    // plist_dict_set_item(response_plist, "model", plist_new_string(vs));
    plist_dict_set_item(response_plist, "model", plist_new_string("Shairport Sync"));
    free(vs);
    // pkString_make(pkString, sizeof(pkString), config.airplay_device_id);
    // plist_dict_set_item(response_plist, "pk", plist_new_string(pkString));
    plist_to_bin(response_plist, &resp->content, &resp->contentlength);
    plist_free(response_plist);
    msg_add_header(resp, "Content-Type", "application/x-apple-binary-plist");
    debug_log_rtsp_message(3, "GET /info Stage 2 Response", resp);
    resp->respcode = 200;
    return;
  }
}

void handle_flushbuffered(rtsp_conn_info *conn, rtsp_message *req, rtsp_message *resp) {
  debug(3, "Connection %d: FLUSHBUFFERED %s : Content-Length %d", conn->connection_number,
        req->path, req->contentlength);
  debug_log_rtsp_message(2, "FLUSHBUFFERED request", req);

  uint64_t flushFromSeq = 0;
  uint64_t flushFromTS = 0;
  uint64_t flushUntilSeq = 0;
  uint64_t flushUntilTS = 0;
  int flushFromValid = 0;
  plist_t messagePlist = plist_from_rtsp_content(req);
  plist_t item = plist_dict_get_item(messagePlist, "flushFromSeq");
  if (item == NULL) {
    debug(2, "Can't find a flushFromSeq");
  } else {
    flushFromValid = 1;
    plist_get_uint_val(item, &flushFromSeq);
    debug(2, "flushFromSeq is %" PRId64 ".", flushFromSeq);
  }

  item = plist_dict_get_item(messagePlist, "flushFromTS");
  if (item == NULL) {
    if (flushFromValid != 0)
      debug(1, "flushFromSeq without flushFromTS!");
    else
      debug(2, "Can't find a flushFromTS");
  } else {
    plist_get_uint_val(item, &flushFromTS);
    if (flushFromValid == 0)
      debug(1, "flushFromTS without flushFromSeq!");
    debug(2, "flushFromTS is %" PRId64 ".", flushFromTS);
  }

  item = plist_dict_get_item(messagePlist, "flushUntilSeq");
  if (item == NULL) {
    debug(1, "Can't find the flushUntilSeq");
  } else {
    plist_get_uint_val(item, &flushUntilSeq);
    debug(2, "flushUntilSeq is %" PRId64 ".", flushUntilSeq);
  }

  item = plist_dict_get_item(messagePlist, "flushUntilTS");
  if (item == NULL) {
    debug(1, "Can't find the flushUntilTS");
  } else {
    plist_get_uint_val(item, &flushUntilTS);
    debug(2, "flushUntilTS is %" PRId64 ".", flushUntilTS);
  }

  debug_mutex_lock(&conn->flush_mutex, 1000, 1);
  // a flush with from... components will not be followed by a setanchor (i.e. a
  // play) if it's a flush that will be followed by a setanchor (i.e. a play)
  // then stop play now.
  if (flushFromValid == 0)
    conn->ap2_play_enabled = 0;

  // add the exact request as made to the linked list (not used for anything but
  // diagnostics now) int flushNow = 0; if (flushFromValid == 0)
  //  flushNow = 1;
  // add_flush_request(flushNow, flushFromSeq, flushFromTS, flushUntilSeq,
  // flushUntilTS, conn);

  // now, if it's an immediate flush, replace the existing request, if any
  // but it if's a deferred flush and there is an existing deferred request,
  // only update the flushUntil stuff -- that seems to preserve
  // the intended semantics

  // so, always replace these
  conn->ap2_flush_until_sequence_number = flushUntilSeq;
  conn->ap2_flush_until_rtp_timestamp = flushUntilTS;

  if ((conn->ap2_flush_requested != 0) && (conn->ap2_flush_from_valid != 0) &&
      (flushFromValid != 0)) {
    // if there is a request already, and it's a deferred request, and the
    // current request is also deferred... do nothing! -- leave the starting
    // point in place. Yeah, yeah, we know de Morgan's Law, but this seems
    // clearer
  } else {
    conn->ap2_flush_from_sequence_number = flushFromSeq;
    conn->ap2_flush_from_rtp_timestamp = flushFromTS;
  }

  conn->ap2_flush_from_valid = flushFromValid;
  conn->ap2_flush_requested = 1;

  // reflect the possibly updated flush request
  // add_flush_request(flushNow, conn->ap2_flush_from_sequence_number,
  // conn->ap2_flush_from_rtp_timestamp, conn->ap2_flush_until_sequence_number,
  // conn->ap2_flush_until_rtp_timestamp, conn);

  debug_mutex_unlock(&conn->flush_mutex, 3);

  if (flushFromValid)
    debug(2, "Deferred Flush Requested");
  else
    debug(2, "Immediate Flush Requested");

  // display_all_flush_requests(conn);

  resp->respcode = 200;
}

void handle_setrate(rtsp_conn_info *conn, rtsp_message *req, rtsp_message *resp) {
  debug(3, "Connection %d: SETRATE %s : Content-Length %d", conn->connection_number, req->path,
        req->contentlength);
  debug_log_rtsp_message(2, "SETRATE request -- unimplemented", req);
  resp->respcode = 501; // Not Implemented
}

void handle_unimplemented_ap1(__attribute((unused)) rtsp_conn_info *conn, rtsp_message *req,
                              rtsp_message *resp) {
  debug_log_rtsp_message(1, "request not recognised for AirPlay 1 operation", req);
  resp->respcode = 501;
}

void handle_setrateanchori(rtsp_conn_info *conn, rtsp_message *req, rtsp_message *resp) {
  debug(3, "Connection %d: SETRATEANCHORI %s :: Content-Length %d", conn->connection_number,
        req->path, req->contentlength);

  plist_t messagePlist = plist_from_rtsp_content(req);

  if (messagePlist != NULL) {
    pthread_cleanup_push(plist_cleanup, (void *)messagePlist);
    plist_t item = plist_dict_get_item(messagePlist, "networkTimeSecs");
    if (item != NULL) {
      plist_t item_2 = plist_dict_get_item(messagePlist, "networkTimeTimelineID");
      if (item_2 == NULL) {
        debug(1, "Can't identify the Clock ID of the player.");
      } else {
        uint64_t nid;
        plist_get_uint_val(item_2, &nid);
        debug(2, "networkTimeTimelineID \"%" PRIx64 "\".", nid);
        conn->networkTimeTimelineID = nid;
      }
      uint64_t networkTimeSecs;
      plist_get_uint_val(item, &networkTimeSecs);
      debug(2, "anchor networkTimeSecs is %" PRIu64 ".", networkTimeSecs);

      item = plist_dict_get_item(messagePlist, "networkTimeFrac");
      uint64_t networkTimeFrac;
      plist_get_uint_val(item, &networkTimeFrac);
      debug(2, "anchor networkTimeFrac is 0%" PRIu64 ".", networkTimeFrac);
      // it looks like the networkTimeFrac is a fraction where the msb is work
      // 1/2, the next 1/4 and so on now, convert the network time and fraction
      // into nanoseconds
      networkTimeFrac = networkTimeFrac >> 32;
      networkTimeFrac = networkTimeFrac * 1000000000;
      networkTimeFrac = networkTimeFrac >> 32; // we should now be left with the ns

      networkTimeSecs = networkTimeSecs * 1000000000; // turn the whole seconds into ns
      uint64_t anchorTimeNanoseconds = networkTimeSecs + networkTimeFrac;

      debug(2, "anchorTimeNanoseconds looks like %" PRIu64 ".", anchorTimeNanoseconds);

      item = plist_dict_get_item(messagePlist, "rtpTime");
      uint64_t rtpTime;

      plist_get_uint_val(item, &rtpTime);
      // debug(1, "anchor rtpTime is %" PRId64 ".", rtpTime);
      uint32_t anchorRTPTime = rtpTime;

      int32_t added_latency = (int32_t)(config.audio_backend_latency_offset * conn->input_rate);
      // debug(1,"anchorRTPTime: %" PRIu32 ", added latency: %" PRId32 ".",
      // anchorRTPTime, added_latency);
      set_ptp_anchor_info(conn, conn->networkTimeTimelineID, anchorRTPTime - added_latency,
                          anchorTimeNanoseconds);
    }

    item = plist_dict_get_item(messagePlist, "rate");
    if (item != NULL) {
      uint64_t rate;
      plist_get_uint_val(item, &rate);
      debug(3, "anchor rate 0x%016" PRIx64 ".", rate);
      debug_mutex_lock(&conn->flush_mutex, 1000, 1);
      pthread_cleanup_push(mutex_unlock, &conn->flush_mutex);
      conn->ap2_rate = rate;
      if ((rate & 1) != 0) {
        debug(2, "Connection %d: Start playing.", conn->connection_number);
        activity_monitor_signify_activity(1);
        conn->ap2_play_enabled = 1;
      } else {
        debug(2, "Connection %d: Stop playing.", conn->connection_number);
        activity_monitor_signify_activity(0);
        conn->ap2_play_enabled = 0;
      }
      pthread_cleanup_pop(1); // unlock the conn->flush_mutex
    }
    pthread_cleanup_pop(1); // plist_free the messagePlist;
  } else {
    debug(1, "missing plist!");
  }
  resp->respcode = 200;
}

void handle_get(rtsp_conn_info *conn, rtsp_message *req, rtsp_message *resp) {
  debug(2, "Connection %d: GET %s :: Content-Length %d", conn->connection_number, req->path,
        req->contentlength);
  debug_log_rtsp_message(2, "GET request", req);
  if (strcmp(req->path, "/info") == 0) {
    handle_get_info(conn, req, resp);
  } else {
    debug(1, "Unhandled GET, path \"%s\".", req->path);
    resp->respcode = 404; // makes sense, right?
  }
}

struct pairings {
  char device_id[PAIR_AP_DEVICE_ID_LEN_MAX];
  uint8_t public_key[32];

  struct pairings *next;
} * pairings;

static struct pairings *pairing_find(const char *device_id) {
  for (struct pairings *pairing = pairings; pairing; pairing = pairing->next) {
    if (strcmp(device_id, pairing->device_id) == 0)
      return pairing;
  }
  return NULL;
}

static void pairing_add(uint8_t public_key[32], const char *device_id) {
  struct pairings *pairing = calloc(1, sizeof(struct pairings));
  snprintf(pairing->device_id, sizeof(pairing->device_id), "%s", device_id);
  memcpy(pairing->public_key, public_key, sizeof(pairing->public_key));

  pairing->next = pairings;
  pairings = pairing;
}

static void pairing_remove(struct pairings *pairing) {
  if (pairing == pairings) {
    pairings = pairing->next;
  } else {
    struct pairings *iter;
    for (iter = pairings; iter && (iter->next != pairing); iter = iter->next)
      ; /* EMPTY */

    if (iter)
      iter->next = pairing->next;
  }

  free(pairing);
}

static int pairing_add_cb(uint8_t public_key[32], const char *device_id,
                          void *cb_arg __attribute__((unused))) {
  debug(1, "pair-add cb for %s", device_id);

  struct pairings *pairing = pairing_find(device_id);
  if (pairing) {
    memcpy(pairing->public_key, public_key, sizeof(pairing->public_key));
    return 0;
  }

  pairing_add(public_key, device_id);
  return 0;
}

static int pairing_remove_cb(uint8_t public_key[32] __attribute__((unused)), const char *device_id,
                             void *cb_arg __attribute__((unused))) {
  debug(1, "pair-remove cb for %s", device_id);

  struct pairings *pairing = pairing_find(device_id);
  if (!pairing) {
    debug(1, "pair-remove callback for unknown device");
    return -1;
  }

  pairing_remove(pairing);
  return 0;
}

static void pairing_list_cb(pair_cb enum_cb, void *enum_cb_arg,
                            void *cb_arg __attribute__((unused))) {
  debug(1, "pair-list cb");

  for (struct pairings *pairing = pairings; pairing; pairing = pairing->next) {
    enum_cb(pairing->public_key, pairing->device_id, enum_cb_arg);
  }
}

void handle_pair_add(rtsp_conn_info *conn __attribute__((unused)), rtsp_message *req,
                     rtsp_message *resp) {
  uint8_t *body = NULL;
  size_t body_len = 0;
  int ret = pair_add(PAIR_SERVER_HOMEKIT, &body, &body_len, pairing_add_cb, NULL,
                     (const uint8_t *)req->content, req->contentlength);
  if (ret < 0) {
    debug(1, "pair-add returned an error");
    resp->respcode = 451;
    return;
  }
  resp->content = (char *)body; // these will be freed when the data is sent
  resp->contentlength = body_len;
  msg_add_header(resp, "Content-Type", "application/octet-stream");
  debug_log_rtsp_message(2, "pair-add response", resp);
}

void handle_pair_list(rtsp_conn_info *conn __attribute__((unused)), rtsp_message *req,
                      rtsp_message *resp) {
  uint8_t *body = NULL;
  size_t body_len = 0;
  int ret = pair_list(PAIR_SERVER_HOMEKIT, &body, &body_len, pairing_list_cb, NULL,
                      (const uint8_t *)req->content, req->contentlength);
  if (ret < 0) {
    debug(1, "pair-list returned an error");
    resp->respcode = 451;
    return;
  }
  resp->content = (char *)body; // these will be freed when the data is sent
  resp->contentlength = body_len;
  msg_add_header(resp, "Content-Type", "application/octet-stream");
  debug_log_rtsp_message(2, "pair-list response", resp);
}

void handle_pair_remove(rtsp_conn_info *conn __attribute__((unused)), rtsp_message *req,
                        rtsp_message *resp) {
  uint8_t *body = NULL;
  size_t body_len = 0;
  int ret = pair_remove(PAIR_SERVER_HOMEKIT, &body, &body_len, pairing_remove_cb, NULL,
                        (const uint8_t *)req->content, req->contentlength);
  if (ret < 0) {
    debug(1, "pair-remove returned an error");
    resp->respcode = 451;
    return;
  }
  resp->content = (char *)body; // these will be freed when the data is sent
  resp->contentlength = body_len;
  msg_add_header(resp, "Content-Type", "application/octet-stream");
  debug_log_rtsp_message(2, "pair-remove response", resp);
}

void handle_pair_verify(rtsp_conn_info *conn, rtsp_message *req, rtsp_message *resp) {
  int ret;
  uint8_t *body = NULL;
  size_t body_len = 0;
  struct pair_result *result;
  debug(2, "Connection %d: pair-verify Content-Length %d", conn->connection_number,
        req->contentlength);

  if (!conn->ap2_control_pairing.verify_ctx) {
    conn->ap2_control_pairing.verify_ctx =
        pair_verify_new(PAIR_SERVER_HOMEKIT, NULL, NULL, NULL, config.airplay_device_id);
    if (!conn->ap2_control_pairing.verify_ctx) {
      debug(1, "Error creating verify context");
      resp->respcode = 500; // Internal Server Error
      goto out;
    }
  }

  ret = pair_verify(&body, &body_len, conn->ap2_control_pairing.verify_ctx,
                    (const uint8_t *)req->content, req->contentlength);
  if (ret < 0) {
    debug(1, pair_verify_errmsg(conn->ap2_control_pairing.verify_ctx));
    resp->respcode = 470; // Connection Authorization Required
    goto out;
  }

  ret = pair_verify_result(&result, conn->ap2_control_pairing.verify_ctx);
  if (ret == 0 && result->shared_secret_len > 0) {
    conn->ap2_control_pairing.cipher_ctx =
        pair_cipher_new(PAIR_SERVER_HOMEKIT, 2, result->shared_secret, result->shared_secret_len);
    if (!conn->ap2_control_pairing.cipher_ctx) {
      debug(1, "Error setting up rtsp control channel ciphering\n");
      goto out;
    }
  }

out:
  resp->content = (char *)body; // these will be freed when the data is sent
  resp->contentlength = body_len;
  if (body)
    msg_add_header(resp, "Content-Type", "application/octet-stream");
  debug_log_rtsp_message(2, "pair-verify response", resp);
}

void handle_pair_setup(rtsp_conn_info *conn, rtsp_message *req, rtsp_message *resp) {
  int ret;
  uint8_t *body = NULL;
  size_t body_len = 0;
  struct pair_result *result;
  debug(2, "Connection %d: pair-setup Content-Length %d", conn->connection_number,
        req->contentlength);

  if (!conn->ap2_control_pairing.setup_ctx) {
    conn->ap2_control_pairing.setup_ctx = pair_setup_new(PAIR_SERVER_HOMEKIT, config.airplay_pin,
                                                         NULL, NULL, config.airplay_device_id);
    if (!conn->ap2_control_pairing.setup_ctx) {
      debug(1, "Error creating setup context");
      resp->respcode = 500; // Internal Server Error
      goto out;
    }
  }

  ret = pair_setup(&body, &body_len, conn->ap2_control_pairing.setup_ctx,
                   (const uint8_t *)req->content, req->contentlength);
  if (ret < 0) {
    debug(1, pair_setup_errmsg(conn->ap2_control_pairing.setup_ctx));
    resp->respcode = 470; // Connection Authorization Required
    goto out;
  }

  ret = pair_setup_result(NULL, &result, conn->ap2_control_pairing.setup_ctx);
  if (ret == 0 && result->shared_secret_len > 0) {
    // Transient pairing completed (pair-setup step 2), prepare encryption, but
    // don't activate yet, the response to this request is still plaintext
    conn->ap2_control_pairing.cipher_ctx =
        pair_cipher_new(PAIR_SERVER_HOMEKIT, 2, result->shared_secret, result->shared_secret_len);
    if (!conn->ap2_control_pairing.cipher_ctx) {
      debug(1, "Error setting up rtsp control channel ciphering\n");
      goto out;
    }
  }

out:
  resp->content = (char *)body; // these will be freed when the data is sent
  resp->contentlength = body_len;
  if (body)
    msg_add_header(resp, "Content-Type", "application/octet-stream");
  debug_log_rtsp_message(2, "pair-setup response", resp);
}

void handle_fp_setup(__attribute__((unused)) rtsp_conn_info *conn, rtsp_message *req,
                     rtsp_message *resp) {
  /* Fairplay magic */
  static uint8_t server_fp_reply1[] = "\x46\x50\x4c\x59\x03\x01\x02\x00\x00\x00"
                                      "\x00\x82\x02\x00\x0f\x9f\x3f\x9e\x0a"
                                      "\x25\x21\xdb\xdf\x31\x2a\xb2\xbf\xb2\x9e"
                                      "\x8d\x23\x2b\x63\x76\xa8\xc8\x18\x70"
                                      "\x1d\x22\xae\x93\xd8\x27\x37\xfe\xaf\x9d"
                                      "\xb4\xfd\xf4\x1c\x2d\xba\x9d\x1f\x49"
                                      "\xca\xaa\xbf\x65\x91\xac\x1f\x7b\xc6\xf7"
                                      "\xe0\x66\x3d\x21\xaf\xe0\x15\x65\x95"
                                      "\x3e\xab\x81\xf4\x18\xce\xed\x09\x5a\xdb"
                                      "\x7c\x3d\x0e\x25\x49\x09\xa7\x98\x31"
                                      "\xd4\x9c\x39\x82\x97\x34\x34\xfa\xcb\x42"
                                      "\xc6\x3a\x1c\xd9\x11\xa6\xfe\x94\x1a"
                                      "\x8a\x6d\x4a\x74\x3b\x46\xc3\xa7\x64\x9e"
                                      "\x44\xc7\x89\x55\xe4\x9d\x81\x55\x00"
                                      "\x95\x49\xc4\xe2\xf7\xa3\xf6\xd5\xba";
  static uint8_t server_fp_reply2[] = "\x46\x50\x4c\x59\x03\x01\x02\x00\x00\x00"
                                      "\x00\x82\x02\x01\xcf\x32\xa2\x57\x14"
                                      "\xb2\x52\x4f\x8a\xa0\xad\x7a\xf1\x64\xe3"
                                      "\x7b\xcf\x44\x24\xe2\x00\x04\x7e\xfc"
                                      "\x0a\xd6\x7a\xfc\xd9\x5d\xed\x1c\x27\x30"
                                      "\xbb\x59\x1b\x96\x2e\xd6\x3a\x9c\x4d"
                                      "\xed\x88\xba\x8f\xc7\x8d\xe6\x4d\x91\xcc"
                                      "\xfd\x5c\x7b\x56\xda\x88\xe3\x1f\x5c"
                                      "\xce\xaf\xc7\x43\x19\x95\xa0\x16\x65\xa5"
                                      "\x4e\x19\x39\xd2\x5b\x94\xdb\x64\xb9"
                                      "\xe4\x5d\x8d\x06\x3e\x1e\x6a\xf0\x7e\x96"
                                      "\x56\x16\x2b\x0e\xfa\x40\x42\x75\xea"
                                      "\x5a\x44\xd9\x59\x1c\x72\x56\xb9\xfb\xe6"
                                      "\x51\x38\x98\xb8\x02\x27\x72\x19\x88"
                                      "\x57\x16\x50\x94\x2a\xd9\x46\x68\x8a";
  static uint8_t server_fp_reply3[] = "\x46\x50\x4c\x59\x03\x01\x02\x00\x00\x00"
                                      "\x00\x82\x02\x02\xc1\x69\xa3\x52\xee"
                                      "\xed\x35\xb1\x8c\xdd\x9c\x58\xd6\x4f\x16"
                                      "\xc1\x51\x9a\x89\xeb\x53\x17\xbd\x0d"
                                      "\x43\x36\xcd\x68\xf6\x38\xff\x9d\x01\x6a"
                                      "\x5b\x52\xb7\xfa\x92\x16\xb2\xb6\x54"
                                      "\x82\xc7\x84\x44\x11\x81\x21\xa2\xc7\xfe"
                                      "\xd8\x3d\xb7\x11\x9e\x91\x82\xaa\xd7"
                                      "\xd1\x8c\x70\x63\xe2\xa4\x57\x55\x59\x10"
                                      "\xaf\x9e\x0e\xfc\x76\x34\x7d\x16\x40"
                                      "\x43\x80\x7f\x58\x1e\xe4\xfb\xe4\x2c\xa9"
                                      "\xde\xdc\x1b\x5e\xb2\xa3\xaa\x3d\x2e"
                                      "\xcd\x59\xe7\xee\xe7\x0b\x36\x29\xf2\x2a"
                                      "\xfd\x16\x1d\x87\x73\x53\xdd\xb9\x9a"
                                      "\xdc\x8e\x07\x00\x6e\x56\xf8\x50\xce";
  static uint8_t server_fp_reply4[] = "\x46\x50\x4c\x59\x03\x01\x02\x00\x00\x00"
                                      "\x00\x82\x02\x03\x90\x01\xe1\x72\x7e"
                                      "\x0f\x57\xf9\xf5\x88\x0d\xb1\x04\xa6\x25"
                                      "\x7a\x23\xf5\xcf\xff\x1a\xbb\xe1\xe9"
                                      "\x30\x45\x25\x1a\xfb\x97\xeb\x9f\xc0\x01"
                                      "\x1e\xbe\x0f\x3a\x81\xdf\x5b\x69\x1d"
                                      "\x76\xac\xb2\xf7\xa5\xc7\x08\xe3\xd3\x28"
                                      "\xf5\x6b\xb3\x9d\xbd\xe5\xf2\x9c\x8a"
                                      "\x17\xf4\x81\x48\x7e\x3a\xe8\x63\xc6\x78"
                                      "\x32\x54\x22\xe6\xf7\x8e\x16\x6d\x18"
                                      "\xaa\x7f\xd6\x36\x25\x8b\xce\x28\x72\x6f"
                                      "\x66\x1f\x73\x88\x93\xce\x44\x31\x1e"
                                      "\x4b\xe6\xc0\x53\x51\x93\xe5\xef\x72\xe8"
                                      "\x68\x62\x33\x72\x9c\x22\x7d\x82\x0c"
                                      "\x99\x94\x45\xd8\x92\x46\xc8\xc3\x59";

  static uint8_t server_fp_header[] = "\x46\x50\x4c\x59\x03\x01\x04\x00\x00\x00\x00\x14";

  resp->respcode = 200; // assume it's handled

  // uint8_t *out;
  // size_t out_len;
  int version_pos = 4;
  int mode_pos = 14;
  int type_pos = 5;
  int seq_pos = 6;
  int setup_message_type = 1;
  int setup1_message_seq = 1;
  int setup2_message_seq = 3;
  int setup2_suffix_len = 20;
  // int ret;

  // response and len are dummy values and can be ignored

  // debug(1, "Version: %02x, mode: %02x, type: %02x, seq: %02x",
  // req->content[version_pos],
  //       req->content[mode_pos], req->content[type_pos],
  //       req->content[seq_pos]);

  if (req->content[version_pos] != 3 || req->content[type_pos] != setup_message_type) {
    debug(1, "Unsupported FP version.");
  }

  char *response = NULL;
  size_t len = 0;

  if (req->content[seq_pos] == setup1_message_seq) {
    // All replies are the same length. -1 to account for the NUL byte at the
    // end.
    len = sizeof(server_fp_reply1) - 1;

    if (req->content[mode_pos] == 0)
      response = memdup(server_fp_reply1, len);
    if (req->content[mode_pos] == 1)
      response = memdup(server_fp_reply2, len);
    if (req->content[mode_pos] == 2)
      response = memdup(server_fp_reply3, len);
    if (req->content[mode_pos] == 3)
      response = memdup(server_fp_reply4, len);
  } else if (req->content[seq_pos] == setup2_message_seq) {
    // -1 to account for the NUL byte at the end.
    len = sizeof(server_fp_header) - 1 + setup2_suffix_len;
    response = malloc(len);
    if (response) {
      memcpy(response, server_fp_header, sizeof(server_fp_header) - 1);
      memcpy(response + sizeof(server_fp_header) - 1,
             req->content + req->contentlength - setup2_suffix_len, setup2_suffix_len);
    }
  }

  if (response == NULL) {
    debug(1, "Cannot create a response.");
  }

  resp->content = response; // these will be freed when the data is sent
  resp->contentlength = len;
  msg_add_header(resp, "Content-Type", "application/octet-stream");
}

void handle_configure(rtsp_conn_info *conn __attribute__((unused)),
                      rtsp_message *req __attribute__((unused)), rtsp_message *resp) {
  uint8_t public_key[32];

  pair_public_key_get(PAIR_SERVER_HOMEKIT, public_key, config.airplay_device_id);

  plist_t response_plist = plist_new_dict();

  plist_dict_set_item(response_plist, "Identifier", plist_new_string(config.airplay_pi));
  plist_dict_set_item(response_plist, "Enable_HK_Access_Control", plist_new_bool(1));
  plist_dict_set_item(response_plist, "PublicKey",
                      plist_new_data((const char *)public_key, sizeof(public_key)));
  plist_dict_set_item(response_plist, "Device_Name", plist_new_string(config.service_name));
  plist_dict_set_item(response_plist, "Access_Control_Level", plist_new_uint(0));

  plist_to_bin(response_plist, &resp->content, &resp->contentlength);
  plist_free(response_plist);

  msg_add_header(resp, "Content-Type", "application/x-apple-binary-plist");
  debug_log_rtsp_message(2, "POST /configure response:", resp);
}

void handle_feedback(__attribute__((unused)) rtsp_conn_info *conn,
                     __attribute__((unused)) rtsp_message *req,
                     __attribute__((unused)) rtsp_message *resp) {
  /* not finished yet
  plist_t payload_plist = plist_new_dict();
  plist_dict_set_item(payload_plist, "type", plist_new_uint(103));
  plist_dict_set_item(payload_plist, "sr", plist_new_real(44100.0));

  plist_t array_plist = plist_new_array();
  plist_array_append_item(array_plist, payload_plist);

  plist_t response_plist = plist_new_dict();
  plist_dict_set_item(response_plist, "streams",array_plist);

  plist_to_bin(response_plist, &resp->content, &resp->contentlength);
  plist_free(response_plist);
  // plist_free(array_plist);
  // plist_free(payload_plist);

  msg_add_header(resp, "Content-Type", "application/x-apple-binary-plist");
  debug_log_rtsp_message(2, "FEEDBACK response:", resp);
  */
}

void handle_command(__attribute__((unused)) rtsp_conn_info *conn, rtsp_message *req,
                    __attribute__((unused)) rtsp_message *resp) {
  debug_log_rtsp_message(3, "POST /command", req);
  if (rtsp_message_contains_plist(req)) {
    plist_t command_dict = NULL;
    plist_from_memory(req->content, req->contentlength, &command_dict);
    if (command_dict != NULL) {
      // we have a plist -- try to get the dict item keyed to
      // "updateMRSupportedCommands"
      plist_t item = plist_dict_get_item(command_dict, "type");
      if (item != NULL) {
        char *typeValue = NULL;
        plist_get_string_val(item, &typeValue);
        if ((typeValue != NULL) && (strcmp(typeValue, "updateMRSupportedCommands") == 0)) {
          item = plist_dict_get_item(command_dict, "params");
          if (item != NULL) {
            // the item should be a dict
            plist_t item_array = plist_dict_get_item(item, "mrSupportedCommandsFromSender");
            if (item_array != NULL) {
              // here we have an array of data items
              uint32_t items = plist_array_get_size(item_array);
              if (items) {
                uint32_t item_number;
                for (item_number = 0; item_number < items; item_number++) {
                  plist_t the_item = plist_array_get_item(item_array, item_number);
                  char *buff = NULL;
                  uint64_t length = 0;
                  plist_get_data_val(the_item, &buff, &length);
                  // debug(1,"Item %d, length: %" PRId64 " bytes", item_number,
                  // length);
                  if ((buff != NULL) && (length >= strlen("bplist00")) &&
                      (strstr(buff, "bplist00") == buff)) {
                    // debug(1,"Contains a plist.");
                    plist_t subsidiary_plist = NULL;
                    plist_from_memory(buff, length, &subsidiary_plist);
                    if (subsidiary_plist) {
                      char *printable_plist = plist_content(subsidiary_plist);
                      if (printable_plist) {
                        debug(3, "\n%s", printable_plist);
                        free(printable_plist);
                      } else {
                        debug(1, "Can't print the plist!");
                      }
                      // plist_free(subsidiary_plist);
                    } else {
                      debug(1, "Can't access the plist!");
                    }
                  }
                  if (buff != NULL)
                    free(buff);
                }
              }
            } else {
              debug(1, "POST /command no mrSupportedCommandsFromSender item.");
            }
          } else {
            debug(1, "POST /command no params dict.");
          }
          resp->respcode = 400; // say it's a bad request
        } else {
          debug(1,
                "POST /command plist type is \"%s\", but "
                "\"updateMRSupportedCommands\" expected.",
                typeValue);
        }
        if (typeValue != NULL)
          free(typeValue);
      } else {
        debug(2, "Could not find a \"type\" item.");
      }

      plist_free(command_dict);
    } else {
      debug(1, "POST /command plist cannot be inputted.");
    }
  } else {
    debug(1, "POST /command contains no plist");
  }
}

void handle_audio_mode(rtsp_conn_info *conn, rtsp_message *req,
                       __attribute__((unused)) rtsp_message *resp) {
  debug(2, "Connection %d: POST %s Content-Length %d", conn->connection_number, req->path,
        req->contentlength);
  debug_log_rtsp_message(2, NULL, req);
}

void handle_post(rtsp_conn_info *conn, rtsp_message *req, rtsp_message *resp) {
  resp->respcode = 200;
  if (strcmp(req->path, "/pair-setup") == 0) {
    handle_pair_setup(conn, req, resp);
  } else if (strcmp(req->path, "/pair-verify") == 0) {
    handle_pair_verify(conn, req, resp);
  } else if (strcmp(req->path, "/pair-add") == 0) {
    handle_pair_add(conn, req, resp);
  } else if (strcmp(req->path, "/pair-remove") == 0) {
    handle_pair_remove(conn, req, resp);
  } else if (strcmp(req->path, "/pair-list") == 0) {
    handle_pair_list(conn, req, resp);
  } else if (strcmp(req->path, "/fp-setup") == 0) {
    handle_fp_setup(conn, req, resp);
  } else if (strcmp(req->path, "/configure") == 0) {
    handle_configure(conn, req, resp);
  } else if (strcmp(req->path, "/feedback") == 0) {
    handle_feedback(conn, req, resp);
  } else if (strcmp(req->path, "/command") == 0) {
    handle_command(conn, req, resp);
  } else if (strcmp(req->path, "/audioMode") == 0) {
    handle_audio_mode(conn, req, resp);
  } else {
    debug(1, "Connection %d: Unhandled POST %s Content-Length %d", conn->connection_number,
          req->path, req->contentlength);
    debug_log_rtsp_message(2, "POST request", req);
  }
}

void handle_setpeers(rtsp_conn_info *conn, rtsp_message *req, rtsp_message *resp) {
  char timing_list_message[4096];
  timing_list_message[0] = 'T';
  timing_list_message[1] = 0;

  plist_t addresses_array = NULL;
  plist_from_memory(req->content, req->contentlength, &addresses_array);
  uint32_t items = plist_array_get_size(addresses_array);
  if (items) {
    uint32_t item;
    for (item = 0; item < items; item++) {
      plist_t n = plist_array_get_item(addresses_array, item);
      char *ip_address = NULL;
      plist_get_string_val(n, &ip_address);
      // debug(1,ip_address);
      strncat(timing_list_message, " ",
              sizeof(timing_list_message) - 1 - strlen(timing_list_message));
      strncat(timing_list_message, ip_address,
              sizeof(timing_list_message) - 1 - strlen(timing_list_message));
      if (ip_address != NULL)
        free(ip_address);
    }
    ptp_send_control_message_string(timing_list_message);
  }
  plist_free(addresses_array);
  resp->respcode = 200;
}

void handle_options(rtsp_conn_info *conn, __attribute__((unused)) rtsp_message *req,
                    rtsp_message *resp) {
  debug_log_rtsp_message(2, "OPTIONS request", req);
  debug(3, "Connection %d: OPTIONS", conn->connection_number);
  resp->respcode = 200;
  msg_add_header(resp, "Public",
                 "ANNOUNCE, SETUP, RECORD, "
                 "PAUSE, FLUSH, FLUSHBUFFERED, TEARDOWN, "
                 "OPTIONS, POST, GET, PUT");
}

void teardown_phase_one(rtsp_conn_info *conn) {
  // this can be called more than once on the same connection --
  // by the player itself but also by the play seesion being killed
  if (conn->player_thread) {
    player_stop(conn);                    // this nulls the player_thread
    activity_monitor_signify_activity(0); // inactive, and should be after command_stop()
  }
  if (conn->session_key) {
    free(conn->session_key);
    conn->session_key = NULL;
  }
}

void teardown_phase_two(rtsp_conn_info *conn) {
  // we are being asked to disconnect
  // this can be called more than once on the same connection --
  // by the player itself but also by the play seesion being killed

  pthread_cancel(*conn->rtp_event_thread);
  close(conn->event_socket);

  // if we are closing a PTP stream only, do this
  if (conn->airplay_stream_category == ptp_stream) {
    if (conn->airplay_gid != NULL) {
      free(conn->airplay_gid);
      conn->airplay_gid = NULL;
    }

    conn->groupContainsGroupLeader = 0;
    config.airplay_statusflags &= (0xffffffff - (1 << 11)); // DeviceSupportsRelay
    build_bonjour_strings(conn);

    mdns_update(NULL, secondary_txt_records);
    if (conn->dacp_active_remote != NULL) {
      free(conn->dacp_active_remote);
      conn->dacp_active_remote = NULL;
    }
    release_play_lock(conn);
  }
}

void handle_teardown_2(rtsp_conn_info *conn, __attribute__((unused)) rtsp_message *req,
                       rtsp_message *resp) {
  debug_log_rtsp_message(2, "TEARDOWN: ", req);
  // if (have_player(conn)) {
  resp->respcode = 200;
  msg_add_header(resp, "Connection", "close");

  plist_t messagePlist = plist_from_rtsp_content(req);
  if (messagePlist != NULL) {
    // now see if the incoming plist contains a "streams" array
    plist_t streams = plist_dict_get_item(messagePlist, "streams");

    if (streams) {
      debug(2, "Connection %d: TEARDOWN a %s.", conn->connection_number,
            get_category_string(conn->airplay_stream_category));
      // we are being asked to close a stream
      teardown_phase_one(conn);
      plist_free(streams);
      debug(2, "Connection %d: TEARDOWN phase one complete", conn->connection_number);
    } else {
      teardown_phase_one(conn); // try to do phase one anyway
      teardown_phase_two(conn);
      debug(2, "Connection %d: TEARDOWN phase two complete", conn->connection_number);
    }
    //} else {
    //  warn("Connection %d TEARDOWN received without having the player (no
    //  ANNOUNCE?)",
    //       conn->connection_number);
    //  resp->respcode = 451;

    plist_free(messagePlist);
    resp->respcode = 200;
  } else {
    debug(1, "Connection %d: missing plist!", conn->connection_number);
    resp->respcode = 451; // don't know what to do here
  }
  // debug(1,"Bogus exit for valgrind -- remember to comment it out!.");
  // exit(EXIT_SUCCESS); //
}

void teardown(rtsp_conn_info *conn) {
  player_stop(conn);
  activity_monitor_signify_activity(0); // inactive, and should be after command_stop()
  if (conn->dacp_active_remote != NULL) {
    free(conn->dacp_active_remote);
    conn->dacp_active_remote = NULL;
  }
}

// void handle_teardown(rtsp_conn_info *conn, __attribute__((unused)) rtsp_message *req,
// rtsp_message *resp) {
//   debug_log_rtsp_message(2, "TEARDOWN request", req);
//   debug(2, "Connection %d: TEARDOWN", conn->connection_number);
//   if (have_play_lock(conn)) {
//     debug(3,
//           "TEARDOWN: synchronously terminating the player thread of RTSP "
//           "conversation thread %d (2).",
//           conn->connection_number);
//     teardown(conn);
//     release_play_lock(conn);
//     resp->respcode = 200;
//     msg_add_header(resp, "Connection", "close");
//     debug(3,
//           "TEARDOWN: successful termination of playing thread of RTSP "
//           "conversation thread %d.",
//           conn->connection_number);
//   } else {
//     warn("Connection %d TEARDOWN received without having the player (no "
//          "ANNOUNCE?)",
//          conn->connection_number);
//     resp->respcode = 451;
//   }
//   // debug(1,"Bogus exit for valgrind -- remember to comment it out!.");
//   // exit(EXIT_SUCCESS);
// }

void handle_flush(rtsp_conn_info *conn, rtsp_message *req, rtsp_message *resp) {
  // TODO -- don't know what this is for in AP2
  debug_log_rtsp_message(2, "FLUSH request", req);
  debug(3, "Connection %d: FLUSH", conn->connection_number);
  char *p = NULL;
  uint32_t rtptime = 0;
  char *hdr = msg_get_header(req, "RTP-Info");

  if (hdr) {
    // debug(1,"FLUSH message received: \"%s\".",hdr);
    // get the rtp timestamp
    p = strstr(hdr, "rtptime=");
    if (p) {
      p = strchr(p, '=');
      if (p)
        rtptime = uatoi(p + 1); // unsigned integer -- up to 2^32-1
    }
  }
  debug(2, "RTSP Flush Requested: %u.", rtptime);
  if (have_play_lock(conn)) {
    // hack -- ignore it for airplay 2

    if (conn->airplay_type != ap_2)

      player_flush(rtptime,
                   conn); // will not crash even it there is no player thread.
    resp->respcode = 200;
  } else {
    warn("Connection %d FLUSH %u received without having the player", conn->connection_number,
         rtptime);
    resp->respcode = 451;
  }
}

void handle_setup_2(rtsp_conn_info *conn, rtsp_message *req, rtsp_message *resp) {
  int err;

  plist_t messagePlist = plist_from_rtsp_content(req);
  plist_t setupResponsePlist = plist_new_dict();
  resp->respcode = 400;

  // see if the incoming plist contains a "streams" array
  plist_t streams = plist_dict_get_item(messagePlist, "streams");
  if (streams == NULL) {
    // no "streams" plist, so it must (?) be an initial setup

    conn->airplay_stream_category = unspecified_stream_category;
    // figure out what category of stream it is, by looking at the plist
    plist_t timingProtocol = plist_dict_get_item(messagePlist, "timingProtocol");

    if (timingProtocol != NULL) {
      char *timingProtocolString = NULL;
      plist_get_string_val(timingProtocol, &timingProtocolString);
      if (timingProtocolString) {
        if (strcmp(timingProtocolString, "PTP") == 0) {
          conn->airplay_stream_category = ptp_stream;
          conn->timing_type = ts_ptp;
          get_play_lock(conn);
        } else if (strcmp(timingProtocolString, "NTP") == 0) {
          conn->airplay_stream_category = ntp_stream;
          conn->timing_type = ts_ntp;
        } else if (strcmp(timingProtocolString, "None") == 0) {
          // now check to see if it's got the "isRemoteControlOnly" item and
          // check it's true
          plist_t isRemoteControlOnly = plist_dict_get_item(messagePlist, "isRemoteControlOnly");
          if (isRemoteControlOnly != NULL) {
            uint8_t isRemoteControlOnlyBoolean = 0;
            plist_get_bool_val(isRemoteControlOnly, &isRemoteControlOnlyBoolean);
            if (isRemoteControlOnlyBoolean != 0) {
              conn->airplay_stream_category = remote_control_stream;
            }
          }
        }

        // here, we know it's an initial setup and we know the kind of setup
        // being requested if it's a full service PTP stream, we get groupUUID,
        // groupContainsGroupLeader and timingPeerList
        if (conn->airplay_stream_category == ptp_stream) {
          ptp_send_control_message_string("T"); // remove all previous history
          debug_log_rtsp_message(2, "SETUP \"PTP\" message", req);
          plist_t groupUUID = plist_dict_get_item(messagePlist, "groupUUID");

          plist_get_string_val(groupUUID, &gid);

          conn->airplay_gid = gid; // it'll be free'd later on...

          // now see if the group contains a group leader
          plist_t groupContainsGroupLeader =
              plist_dict_get_item(messagePlist, "groupContainsGroupLeader");
          if (groupContainsGroupLeader) {
            uint8_t value = 0;
            plist_get_bool_val(groupContainsGroupLeader, &value);
            conn->groupContainsGroupLeader = value;
          }

          char timing_list_message[4096];
          timing_list_message[0] = 'T';
          timing_list_message[1] = 0;

          plist_t timing_peer_info = plist_dict_get_item(messagePlist, "timingPeerInfo");
          if (timing_peer_info) {
            // first, get the incoming plist.
            plist_t addresses_array = plist_dict_get_item(timing_peer_info, "Addresses");
            if (addresses_array) {
              // iterate through the array of items
              uint32_t items = plist_array_get_size(addresses_array);
              if (items) {
                uint32_t item;
                for (item = 0; item < items; item++) {
                  plist_t n = plist_array_get_item(addresses_array, item);
                  char *ip_address = NULL;
                  plist_get_string_val(n, &ip_address);
                  // debug(1, "Timing peer: %s", ip_address);
                  strncat(timing_list_message, " ",
                          sizeof(timing_list_message) - 1 - strlen(timing_list_message));
                  strncat(timing_list_message, ip_address,
                          sizeof(timing_list_message) - 1 - strlen(timing_list_message));
                  free(ip_address);
                }
              }
            }
            // make up the timing peer info list part of the response...
            // debug(1,"Create timingPeerInfoPlist");
            plist_t timingPeerInfoPlist = plist_new_dict();
            plist_t addresses = plist_new_array(); // to hold the device's interfaces
            plist_array_append_item(addresses, plist_new_string(conn->self_ip_string));
            //            debug(1,"self ip: \"%s\"", conn->self_ip_string);

            struct ifaddrs *addrs, *iap;
            getifaddrs(&addrs);
            for (iap = addrs; iap != NULL; iap = iap->ifa_next) {
              // debug(1, "Interface index %d, name:
              // \"%s\"",if_nametoindex(iap->ifa_name), iap->ifa_name);
              if ((iap->ifa_addr) && (iap->ifa_netmask) && (iap->ifa_flags & IFF_UP) &&
                  ((iap->ifa_flags & IFF_LOOPBACK) == 0)) {
                char buf[INET6_ADDRSTRLEN + 1]; // +1 for a NUL
                memset(buf, 0, sizeof(buf));
                if (iap->ifa_addr->sa_family == AF_INET6) {
                  struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)(iap->ifa_addr);
                  inet_ntop(AF_INET6, (void *)&addr6->sin6_addr, buf, sizeof(buf));
                  plist_array_append_item(addresses, plist_new_string(buf));
                  // debug(1, "Own address IPv6: %s", buf);

                  // strncat(timing_list_message, " ",
                  // sizeof(timing_list_message) - 1 -
                  // strlen(timing_list_message)); strncat(timing_list_message,
                  // buf, sizeof(timing_list_message) - 1 -
                  // strlen(timing_list_message));
                } else {
                  struct sockaddr_in *addr = (struct sockaddr_in *)(iap->ifa_addr);
                  inet_ntop(AF_INET, (void *)&addr->sin_addr, buf, sizeof(buf));
                  plist_array_append_item(addresses, plist_new_string(buf));
                  // debug(1, "Own address IPv4: %s", buf);

                  // strncat(timing_list_message, " ",
                  // sizeof(timing_list_message) - 1 -
                  // strlen(timing_list_message)); strncat(timing_list_message,
                  // buf, sizeof(timing_list_message) - 1 -
                  // strlen(timing_list_message));
                }
              }
            }
            freeifaddrs(addrs);

            // debug(1,"initial timing peer command: \"%s\".",
            // timing_list_message);
            ptp_send_control_message_string(timing_list_message);
            plist_dict_set_item(timingPeerInfoPlist, "Addresses", addresses);
            plist_dict_set_item(timingPeerInfoPlist, "ID", plist_new_string(conn->self_ip_string));
            plist_dict_set_item(setupResponsePlist, "timingPeerInfo", timingPeerInfoPlist);
            // get a port to use as an event port
            // bind a new TCP port and get a socket
            conn->local_event_port = 0; // any port
            int err = bind_socket_and_port(SOCK_STREAM, conn->connection_ip_family,
                                           conn->self_ip_string, conn->self_scope_id,
                                           &conn->local_event_port, &conn->event_socket);

            pthread_create(conn->rtp_event_thread, NULL, &rtp_event_receiver, (void *)conn);

            plist_dict_set_item(setupResponsePlist, "eventPort",
                                plist_new_uint(conn->local_event_port));
            plist_dict_set_item(setupResponsePlist, "timingPort",
                                plist_new_uint(0)); // dummy
            cancel_all_RTSP_threads(unspecified_stream_category,
                                    conn->connection_number); // kill all the other listeners

            config.airplay_statusflags |= 1 << 11; // DeviceSupportsRelay
            build_bonjour_strings(conn);
            mdns_update(NULL, secondary_txt_records);

            resp->respcode = 200;
          }
        } else if (conn->airplay_stream_category == ntp_stream) {
          warn("Shairport Sync can not handle NTP streams.");
        } else if (conn->airplay_stream_category == remote_control_stream) {
          // get a port to use as an event port
          // bind a new TCP port and get a socket
          conn->local_event_port = 0; // any port
          int err = bind_socket_and_port(SOCK_STREAM, conn->connection_ip_family,
                                         conn->self_ip_string, conn->self_scope_id,
                                         &conn->local_event_port, &conn->event_socket);

          pthread_create(conn->rtp_event_thread, NULL, &rtp_event_receiver, (void *)conn);

          plist_dict_set_item(setupResponsePlist, "eventPort",
                              plist_new_uint(conn->local_event_port));
          plist_dict_set_item(setupResponsePlist, "timingPort", plist_new_uint(0));
          cancel_all_RTSP_threads(remote_control_stream,
                                  conn->connection_number); // kill all the other remote control
                                                            // listeners
          resp->respcode = 200;
        }
      }
    }
  } else {
    if (conn->airplay_stream_category == ptp_stream) {
      // get stream[0]
      plist_t stream0 = plist_array_get_item(streams, 0);

      plist_t streams_array = plist_new_array(); // to hold the ports and stuff
      plist_t stream0dict = plist_new_dict();
      // more stuff
      // set up a UDP control stream and thread and a UDP or TCP audio stream
      // and thread

      // bind a new UDP port and get a socket
      conn->local_ap2_control_port = 0; // any port
      err = bind_socket_and_port(SOCK_DGRAM, conn->connection_ip_family, conn->self_ip_string,
                                 conn->self_scope_id, &conn->local_ap2_control_port,
                                 &conn->ap2_control_socket);

      pthread_create(&conn->rtp_ap2_control_thread, NULL, &rtp_ap2_control_receiver, (void *)conn);

      // get the session key

      plist_t item = plist_dict_get_item(stream0, "shk"); // session key
      uint64_t item_value = 0;
      plist_get_data_val(item, (char **)&conn->session_key, &item_value);

      // get the DACP-ID and Active Remote for remote control stuff

      char *ar = msg_get_header(req, "Active-Remote");
      if (ar) {
        // get the active remote
        conn->dacp_active_remote = strdup(ar);
      } else {
        conn->dacp_active_remote = NULL;
      }

      ar = msg_get_header(req, "DACP-ID");
      if (ar) {
        conn->dacp_id = strdup(ar);
      } else {
        conn->dacp_id = NULL;
      }

      // now, get the type of the stream.
      item = plist_dict_get_item(stream0, "type");
      item_value = 0;
      plist_get_uint_val(item, &item_value);

      switch (item_value) {
        case 96: {
          conn->airplay_stream_type = realtime_stream;
          // bind a new UDP port and get a socket
          conn->local_realtime_audio_port = 0; // any port
          err = bind_socket_and_port(SOCK_DGRAM, conn->connection_ip_family, conn->self_ip_string,
                                     conn->self_scope_id, &conn->local_realtime_audio_port,
                                     &conn->realtime_audio_socket);

          pthread_create(&conn->rtp_realtime_audio_thread, NULL, &rtp_realtime_audio_receiver,
                         (void *)conn);

          plist_dict_set_item(stream0dict, "type", plist_new_uint(96));
          plist_dict_set_item(stream0dict, "dataPort",
                              plist_new_uint(conn->local_realtime_audio_port));

          conn->stream.type = ast_apple_lossless;

          // Set reasonable connection defaults
          conn->stream.fmtp[0] = 96;
          conn->stream.fmtp[1] = 352;
          conn->stream.fmtp[2] = 0;
          conn->stream.fmtp[3] = 16;
          conn->stream.fmtp[4] = 40;
          conn->stream.fmtp[5] = 10;
          conn->stream.fmtp[6] = 14;
          conn->stream.fmtp[7] = 2;
          conn->stream.fmtp[8] = 255;
          conn->stream.fmtp[9] = 0;
          conn->stream.fmtp[10] = 0;
          conn->stream.fmtp[11] = 44100;

          // set the parameters of the player (as distinct from the parameters of
          // the decoder -- that's done later).
          conn->max_frames_per_packet = conn->stream.fmtp[1]; // number of audio frames per packet.
          conn->input_rate = conn->stream.fmtp[11];
          conn->input_num_channels = conn->stream.fmtp[7];
          conn->input_bit_depth = conn->stream.fmtp[3];
          conn->input_bytes_per_frame =
              conn->input_num_channels * ((conn->input_bit_depth + 7) / 8);

          player_prepare_to_play(conn);
          player_play(conn);

          conn->rtp_running = 1; // hack!
        } break;
        case 103: {
          conn->airplay_stream_type = buffered_stream;
          // get needed stuff

          // bind a new TCP port and get a socket
          conn->local_buffered_audio_port = 0; // any port
          err = bind_socket_and_port(SOCK_STREAM, conn->connection_ip_family, conn->self_ip_string,
                                     conn->self_scope_id, &conn->local_buffered_audio_port,
                                     &conn->buffered_audio_socket);

          // hack.
          conn->max_frames_per_packet = 352; // number of audio frames per packet.
          conn->input_rate = 44100;          // we are stuck with this for the moment.
          conn->input_num_channels = 2;
          conn->input_bit_depth = 16;
          conn->input_bytes_per_frame =
              conn->input_num_channels * ((conn->input_bit_depth + 7) / 8);

          player_prepare_to_play(conn); // get capabilities of DAC before creating
                                        // the buffered audio thread

          pthread_create(&conn->rtp_buffered_audio_thread, NULL, &rtp_buffered_audio_processor,
                         (void *)conn);

          plist_dict_set_item(stream0dict, "type", plist_new_uint(103));
          plist_dict_set_item(stream0dict, "dataPort",
                              plist_new_uint(conn->local_buffered_audio_port));
          plist_dict_set_item(stream0dict, "audioBufferSize",
                              plist_new_uint(conn->ap2_audio_buffer_size));

          // this should be cancelled by an activity_monitor_signify_activity(1)
          // call in the SETRATEANCHORI handler, which should come up right away
          player_play(conn);

          conn->rtp_running = 1; // hack!
        } break;
        default:
          debug_log_rtsp_message(1, "Unhandled stream type incoming message", req);
      }

      plist_dict_set_item(stream0dict, "controlPort",
                          plist_new_uint(conn->local_ap2_control_port));

      plist_array_append_item(streams_array, stream0dict);
      plist_dict_set_item(setupResponsePlist, "streams", streams_array);
      resp->respcode = 200;
    } else if (conn->airplay_stream_category == remote_control_stream) {
      debug(2, "Connection %d: SETUP: Remote Control Stream received.", conn->connection_number);
      debug_log_rtsp_message(2, "Remote Control Stream stream (second) message", req);
      plist_t coreResponseDict = plist_new_dict();
      plist_dict_set_item(coreResponseDict, "streamID", plist_new_uint(1));
      plist_dict_set_item(coreResponseDict, "type", plist_new_uint(130));
      plist_t coreResponseArray = plist_new_array();
      plist_array_append_item(coreResponseArray, coreResponseDict);
      plist_dict_set_item(setupResponsePlist, "streams", coreResponseArray);
      resp->respcode = 200;
    }
  }

  if (resp->respcode == 200) {
    plist_to_bin(setupResponsePlist, &resp->content, &resp->contentlength);
    plist_free(setupResponsePlist);
    msg_add_header(resp, "Content-Type", "application/x-apple-binary-plist");
  }
}

void handle_set_parameter_parameter(rtsp_conn_info *conn, rtsp_message *req,
                                    __attribute__((unused)) rtsp_message *resp) {
  char *cp = req->content;
  int cp_left = req->contentlength;

  char *next;
  while (cp_left && cp) {
    next = nextline(cp, cp_left);
    // note: "next" will return NULL if there is no \r or \n or \r\n at the end
    // of this but we are always guaranteed that if cp is not null, it will be
    // pointing to something NUL-terminated

    if (next)
      cp_left -= (next - cp);
    else
      cp_left = 0;

    if (!strncmp(cp, "volume: ", strlen("volume: "))) {
      float volume = atof(cp + strlen("volume: "));
      debug(2, "Connection %d: request to set AirPlay Volume to: %f.", conn->connection_number,
            volume);
      // if we are playing, go ahead and change the volume
      if (try_to_hold_play_lock(conn) == 0) {
        player_volume(volume, conn);
        release_hold_on_play_lock(conn);
      } else {
        // otherwise use the volume as the initial setting for when/if the
        // player starts
        conn->initial_airplay_volume = volume;
        conn->initial_airplay_volume_set = 1;
      }
    } else if (strncmp(cp, "progress: ", strlen("progress: ")) ==
               0) { // this can be sent even when metadata is not solicited
    } else {
      debug(1, "Connection %d, unrecognised parameter: \"%s\" (%d)\n", conn->connection_number, cp,
            strlen(cp));
    }
    cp = next;
  }
}

static void handle_get_parameter(__attribute__((unused)) rtsp_conn_info *conn, rtsp_message *req,
                                 rtsp_message *resp) {
  // debug(1, "Connection %d: GET_PARAMETER", conn->connection_number);
  // debug_print_msg_headers(1,req);
  // debug_print_msg_content(1,req);

  if ((req->content) && (req->contentlength == strlen("volume\r\n")) &&
      strstr(req->content, "volume") == req->content) {
    debug(2, "Connection %d: Current volume (%.6f) requested", conn->connection_number,
          config.airplay_volume);
    char *p = malloc(128); // will be automatically deallocated with the response is deleted
    if (p) {
      resp->content = p;
      resp->contentlength = snprintf(p, 128, "\r\nvolume: %.6f\r\n", config.airplay_volume);
    } else {
      debug(1, "Couldn't allocate space for a response.");
    }
  }
  resp->respcode = 200;
}

static void handle_set_parameter(rtsp_conn_info *conn, rtsp_message *req, rtsp_message *resp) {
  char *ct = msg_get_header(req, "Content-Type");

  if (ct) {
    // debug(2, "SET_PARAMETER Content-Type:\"%s\".", ct);

    if (!strncmp(ct, "text/parameters", 15)) {
      // debug(2, "received parameters in SET_PARAMETER request.");
      handle_set_parameter_parameter(conn, req,
                                     resp); // this could be volume or progress
    }
  }
  resp->respcode = 200;
}

static void handle_announce(rtsp_conn_info *conn, rtsp_message *req, rtsp_message *resp) {
  debug(1, "Connection %d: ANNOUNCE - AirPlay 1 not supported, %p %p", req, resp);
}

static struct method_handler {
  char *method;
  void (*ap1_handler)(rtsp_conn_info *conn, rtsp_message *req,
                      rtsp_message *resp); // for AirPlay 1
  void (*ap2_handler)(rtsp_conn_info *conn, rtsp_message *req,
                      rtsp_message *resp); // for AirPlay 2
} method_handlers[] = {{"OPTIONS", handle_options, handle_options},
                       {"ANNOUNCE", handle_announce, handle_announce},
                       {"FLUSH", handle_flush, handle_flush},
                       {"TEARDOWN", NULL, handle_teardown_2},
                       {"SETUP", NULL, handle_setup_2},
                       {"GET_PARAMETER", handle_get_parameter, handle_get_parameter},
                       {"SET_PARAMETER", handle_set_parameter, handle_set_parameter},
                       {"RECORD", NULL, handle_record_2},
                       {"GET", handle_get, handle_get},
                       {"POST", handle_post, handle_post},
                       {"SETPEERS", NULL, handle_setpeers},
                       {"SETRATEANCHORTI", NULL, handle_setrateanchori},
                       {"FLUSHBUFFERED", NULL, handle_flushbuffered},
                       {"SETRATE", NULL, handle_setrate},
                       {NULL, NULL, NULL}};

static void apple_challenge(int fd, rtsp_message *req, rtsp_message *resp) {
  char *hdr = msg_get_header(req, "Apple-Challenge");
  if (!hdr)
    return;
  SOCKADDR fdsa;
  socklen_t sa_len = sizeof(fdsa);
  getsockname(fd, (struct sockaddr *)&fdsa, &sa_len);

  int chall_len;
  uint8_t *chall = base64_dec(hdr, &chall_len);
  if (chall == NULL)
    die("null chall in apple_challenge");
  uint8_t buf[48], *bp = buf;
  int i;
  memset(buf, 0, sizeof(buf));

  if (chall_len > 16) {
    warn("oversized Apple-Challenge!");
    free(chall);
    return;
  }
  memcpy(bp, chall, chall_len);
  free(chall);
  bp += chall_len;

#ifdef AF_INET6
  if (fdsa.SAFAMILY == AF_INET6) {
    struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)(&fdsa);
    memcpy(bp, sa6->sin6_addr.s6_addr, 16);
    bp += 16;
  } else
#endif
  {
    struct sockaddr_in *sa = (struct sockaddr_in *)(&fdsa);
    memcpy(bp, &sa->sin_addr.s_addr, 4);
    bp += 4;
  }

  for (i = 0; i < 6; i++)
    *bp++ = config.hw_addr[i];

  int buflen, resplen;
  buflen = bp - buf;
  if (buflen < 0x20)
    buflen = 0x20;

  uint8_t *challresp = rsa_apply(buf, buflen, &resplen, RSA_MODE_AUTH);
  char *encoded = base64_enc(challresp, resplen);
  if (encoded == NULL)
    die("could not allocate memory for \"encoded\"");
  // strip the padding.
  char *padding = strchr(encoded, '=');
  if (padding)
    *padding = 0;

  msg_add_header(resp, "Apple-Response",
                 encoded); // will be freed when the response is freed.
  free(challresp);
  free(encoded);
}

static char *make_nonce(void) {
  uint8_t random[8];
  int fd = open("/dev/urandom", O_RDONLY);
  if (fd < 0)
    die("could not open /dev/urandom!");
  // int ignore =
  if (read(fd, random, sizeof(random)) != sizeof(random))
    debug(1, "Error reading /dev/urandom");
  close(fd);
  return base64_enc(random, 8); // returns a pointer to malloc'ed memory
}

static int rtsp_auth(char **nonce, rtsp_message *req, rtsp_message *resp) {
  if (!config.password)
    return 0;
  if (!*nonce) {
    *nonce = make_nonce();
    goto authenticate;
  }

  char *hdr = msg_get_header(req, "Authorization");
  if (!hdr || strncmp(hdr, "Digest ", 7))
    goto authenticate;

  char *realm = strstr(hdr, "realm=\"");
  char *username = strstr(hdr, "username=\"");
  char *response = strstr(hdr, "response=\"");
  char *uri = strstr(hdr, "uri=\"");

  if (!realm || !username || !response || !uri)
    goto authenticate;

  char *quote;
  realm = strchr(realm, '"') + 1;
  if (!(quote = strchr(realm, '"')))
    goto authenticate;
  *quote = 0;
  username = strchr(username, '"') + 1;
  if (!(quote = strchr(username, '"')))
    goto authenticate;
  *quote = 0;
  response = strchr(response, '"') + 1;
  if (!(quote = strchr(response, '"')))
    goto authenticate;
  *quote = 0;
  uri = strchr(uri, '"') + 1;
  if (!(quote = strchr(uri, '"')))
    goto authenticate;
  *quote = 0;

  uint8_t digest_urp[16], digest_mu[16], digest_total[16];

  MD5_CTX ctx;

  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  MD5_Init(&ctx);
  MD5_Update(&ctx, username, strlen(username));
  MD5_Update(&ctx, ":", 1);
  MD5_Update(&ctx, realm, strlen(realm));
  MD5_Update(&ctx, ":", 1);
  MD5_Update(&ctx, config.password, strlen(config.password));
  MD5_Final(digest_urp, &ctx);
  MD5_Init(&ctx);
  MD5_Update(&ctx, req->method, strlen(req->method));
  MD5_Update(&ctx, ":", 1);
  MD5_Update(&ctx, uri, strlen(uri));
  MD5_Final(digest_mu, &ctx);
  pthread_setcancelstate(oldState, NULL);

  int i;
  unsigned char buf[33];
  for (i = 0; i < 16; i++)
    snprintf((char *)buf + 2 * i, 3, "%02x", digest_urp[i]);
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  MD5_Init(&ctx);
  MD5_Update(&ctx, buf, 32);
  MD5_Update(&ctx, ":", 1);
  MD5_Update(&ctx, *nonce, strlen(*nonce));
  MD5_Update(&ctx, ":", 1);
  for (i = 0; i < 16; i++)
    snprintf((char *)buf + 2 * i, 3, "%02x", digest_mu[i]);
  MD5_Update(&ctx, buf, 32);
  MD5_Final(digest_total, &ctx);
  pthread_setcancelstate(oldState, NULL);
  for (i = 0; i < 16; i++)
    snprintf((char *)buf + 2 * i, 3, "%02x", digest_total[i]);

  if (!strcmp(response, (const char *)buf))
    return 0;
  warn("Password authorization failed.");

authenticate:
  resp->respcode = 401;
  int hdrlen = strlen(*nonce) + 40;
  char *authhdr = malloc(hdrlen);
  snprintf(authhdr, hdrlen, "Digest realm=\"raop\", nonce=\"%s\"", *nonce);
  msg_add_header(resp, "WWW-Authenticate", authhdr);
  free(authhdr);
  return 1;
}

void rtsp_conversation_thread_cleanup_function(void *arg) {
  rtsp_conn_info *conn = (rtsp_conn_info *)arg;
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);

  debug(2, "Connection %d: rtsp_conversation_thread_func_cleanup_function called.",
        conn->connection_number);

  // AP2
  teardown_phase_one(conn);
  teardown_phase_two(conn);

  debug(3, "Connection %d terminating:Closing timing, control and audio sockets...",
        conn->connection_number);
  if (conn->control_socket) {
    close(conn->control_socket);
  }
  if (conn->timing_socket) {
    close(conn->timing_socket);
  }
  if (conn->audio_socket) {
    close(conn->audio_socket);
  }

  if (conn->fd > 0) {
    debug(3, "Connection %d terminating: closing fd %d.", conn->connection_number, conn->fd);
    close(conn->fd);
    debug(3, "Connection %d terminating: closed fd %d.", conn->connection_number, conn->fd);

    debug(2, "Connection %d: terminating connection from %s:%u to self at %s:%u.",
          conn->connection_number, conn->client_ip_string, conn->client_rtsp_port,
          conn->self_ip_string, conn->self_rtsp_port);
  }
  if (conn->auth_nonce) {
    free(conn->auth_nonce);
    conn->auth_nonce = NULL;
  }

  buf_drain(&conn->ap2_control_pairing.plain_buf, -1);
  buf_drain(&conn->ap2_control_pairing.encrypted_buf, -1);
  pair_setup_free(conn->ap2_control_pairing.setup_ctx);
  pair_verify_free(conn->ap2_control_pairing.verify_ctx);
  pair_cipher_free(conn->ap2_control_pairing.cipher_ctx);
  if (conn->airplay_gid) {
    free(conn->airplay_gid);
    conn->airplay_gid = NULL;
  }

  rtp_terminate(conn);

  if (conn->dacp_id) {
    free(conn->dacp_id);
    conn->dacp_id = NULL;
  }

  if (conn->UserAgent) {
    free(conn->UserAgent);
    conn->UserAgent = NULL;
  }

  // remove flow control and mutexes
  int rc = pthread_mutex_destroy(&conn->volume_control_mutex);
  if (rc)
    debug(1, "Connection %d: error %d destroying volume_control_mutex.", conn->connection_number,
          rc);
  rc = pthread_cond_destroy(&conn->flowcontrol);
  if (rc)
    debug(1, "Connection %d: error %d destroying flow control condition variable.",
          conn->connection_number, rc);
  rc = pthread_mutex_destroy(&conn->ab_mutex);
  if (rc)
    debug(1, "Connection %d: error %d destroying ab_mutex.", conn->connection_number, rc);
  rc = pthread_mutex_destroy(&conn->flush_mutex);
  if (rc)
    debug(1, "Connection %d: error %d destroying flush_mutex.", conn->connection_number, rc);

  debug(3, "Cancel watchdog thread.");
  pthread_cancel(conn->player_watchdog_thread);
  debug(3, "Join watchdog thread.");
  pthread_join(conn->player_watchdog_thread, NULL);
  debug(3, "Delete watchdog mutex.");
  pthread_mutex_destroy(&conn->watchdog_mutex);

  // debug(3, "Connection %d: Checking play lock.", conn->connection_number);
  // release_play_lock(conn);

  debug(2, "Connection %d: Closed.", conn->connection_number);
  conn->running = 0;
  pthread_setcancelstate(oldState, NULL);
}

void msg_cleanup_function(void *arg) {
  // debug(3, "msg_cleanup_function called.");
  msg_free((rtsp_message **)arg);
}

static void *rtsp_conversation_thread_func(void *pconn) {
  rtsp_conn_info *conn = pconn;

  // create the watchdog mutex, initialise the watchdog time and start the
  // watchdog thread;
  conn->watchdog_bark_time = get_absolute_time_in_ns();
  pthread_mutex_init(&conn->watchdog_mutex, NULL);
  pthread_create(&conn->player_watchdog_thread, NULL, &player_watchdog_thread_code, (void *)conn);

  int rc = pthread_mutex_init(&conn->flush_mutex, NULL);
  if (rc)
    die("Connection %d: error %d initialising flush_mutex.", conn->connection_number, rc);
  rc = pthread_mutex_init(&conn->ab_mutex, NULL);
  if (rc)
    die("Connection %d: error %d initialising ab_mutex.", conn->connection_number, rc);
  rc = pthread_cond_init(&conn->flowcontrol, NULL);
  if (rc)
    die("Connection %d: error %d initialising flow control condition variable.",
        conn->connection_number, rc);
  rc = pthread_mutex_init(&conn->volume_control_mutex, NULL);
  if (rc)
    die("Connection %d: error %d initialising volume_control_mutex.", conn->connection_number, rc);

  // nothing before this is cancellable
  pthread_cleanup_push(rtsp_conversation_thread_cleanup_function, (void *)conn);

  rtp_initialise(conn);
  char *hdr = NULL;

  enum rtsp_read_request_response reply;

  int rtsp_read_request_attempt_count = 1; // 1 means exit immediately
  rtsp_message *req, *resp;

  conn->ap2_audio_buffer_size = 1024 * 1024 * 8;

  while (conn->stop == 0) {
    int debug_level = 3; // for printing the request and response
    reply = rtsp_read_request(conn, &req);
    if (reply == rtsp_read_request_response_ok) {
      pthread_cleanup_push(msg_cleanup_function, (void *)&req);
      resp = msg_init();
      pthread_cleanup_push(msg_cleanup_function, (void *)&resp);
      resp->respcode = 501; // Not Implemented
      int dl = debug_level;
      if ((strcmp(req->method, "OPTIONS") == 0) ||
          (strcmp(req->method, "POST") == 0)) // the options message is very common, so don't log
                                              // it until level 3
        dl = 3;
      debug(dl, "Connection %d: Received an RTSP Packet of type \"%s\":", conn->connection_number,
            req->method),
          debug_print_msg_headers(dl, req);
      apple_challenge(conn->fd, req, resp);
      hdr = msg_get_header(req, "CSeq");
      if (hdr)
        msg_add_header(resp, "CSeq", hdr);
      //      msg_add_header(resp, "Audio-Jack-Status", "connected;
      //      type=analog");

      msg_add_header(resp, "Server", "AirTunes/366.0");

      if ((conn->authorized == 1) || (rtsp_auth(&conn->auth_nonce, req, resp)) == 0) {
        conn->authorized = 1; // it must have been authorized or didn't need a password
        struct method_handler *mh;
        int method_selected = 0;
        for (mh = method_handlers; mh->method; mh++) {
          if (!strcmp(mh->method, req->method)) {
            method_selected = 1;

            if (conn->airplay_type == ap_1)
              mh->ap1_handler(conn, req, resp);
            else
              mh->ap2_handler(conn, req, resp);

            break;
          }
        }
        if (method_selected == 0) {
          debug(1,
                "Connection %d: Unrecognised and unhandled rtsp request "
                "\"%s\". HTTP Response Code "
                "501 (\"Not Implemented\") returned.",
                conn->connection_number, req->method);

          int y = req->contentlength;
          if (y > 0) {
            char obf[4096];
            if (y > 4096)
              y = 4096;
            char *p = req->content;
            char *obfp = obf;
            int obfc;
            for (obfc = 0; obfc < y; obfc++) {
              snprintf(obfp, 3, "%02X", (unsigned int)*p);
              p++;
              obfp += 2;
            };
            *obfp = 0;
            debug(1, "Content: \"%s\".", obf);
          }
        }
      }
      debug(dl, "Connection %d: RTSP Response:", conn->connection_number);
      debug_print_msg_headers(dl, resp);

      if (conn->stop == 0) {
        int err = msg_write_response(conn, resp);
        if (err) {
          debug(1,
                "Connection %d: Unable to write an RTSP message response. "
                "Terminating the "
                "connection.",
                conn->connection_number);
          struct linger so_linger;
          so_linger.l_onoff = 1; // "true"
          so_linger.l_linger = 0;
          err = setsockopt(conn->fd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof so_linger);
          if (err)
            debug(1, "Could not set the RTSP socket to abort due to a write "
                     "error on closing.");
          conn->stop = 1;
          // if (debuglev >= 1)
          //  debuglev = 3; // see what happens next
        }
      }
      pthread_cleanup_pop(1);
      pthread_cleanup_pop(1);
    } else {
      int tstop = 0;
      if (reply == rtsp_read_request_response_immediate_shutdown_requested)
        tstop = 1;
      else if ((reply == rtsp_read_request_response_channel_closed) ||
               (reply == rtsp_read_request_response_read_error)) {
        if (conn->player_thread) {
          rtsp_read_request_attempt_count--;
          if (rtsp_read_request_attempt_count == 0) {
            tstop = 1;
            if (reply == rtsp_read_request_response_read_error) {
              struct linger so_linger;
              so_linger.l_onoff = 1; // "true"
              so_linger.l_linger = 0;
              int err = setsockopt(conn->fd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof so_linger);
              if (err)
                debug(1, "Could not set the RTSP socket to abort due to a read "
                         "error on closing.");
            }
            // debuglev = 3; // see what happens next
          } else {
            if (reply == rtsp_read_request_response_channel_closed)
              debug(2,
                    "Connection %d: RTSP channel unexpectedly closed -- will "
                    "try again %d time(s).",
                    conn->connection_number, rtsp_read_request_attempt_count);
            if (reply == rtsp_read_request_response_read_error)
              debug(2,
                    "Connection %d: RTSP channel read error -- will try again "
                    "%d time(s).",
                    conn->connection_number, rtsp_read_request_attempt_count);
            usleep(20000);
          }
        } else {
          tstop = 1;
        }
      } else if (reply == rtsp_read_request_response_bad_packet) {
        char *response_text = "RTSP/1.0 400 Bad Request\r\nServer: AirTunes/105.1\r\n\r\n";
        ssize_t reply = write(conn->fd, response_text, strlen(response_text));
        if (reply == -1) {
          char errorstring[1024];
          strerror_r(errno, (char *)errorstring, sizeof(errorstring));
          debug(1,
                "rtsp_read_request_response_bad_packet write response error "
                "%d: \"%s\".",
                errno, (char *)errorstring);
        } else if (reply != (ssize_t)strlen(response_text)) {
          debug(1,
                "rtsp_read_request_response_bad_packet write %d bytes "
                "requested but %d written.",
                strlen(response_text), reply);
        }
      } else {
        debug(1, "Connection %d: rtsp_read_request error %d, packet ignored.",
              conn->connection_number, (int)reply);
      }
      if (tstop) {
        debug(3, "Connection %d: Terminate RTSP connection.", conn->connection_number);
        conn->stop = 1;
      }
    }
  }
  pthread_cleanup_pop(1);
  pthread_exit(NULL);
  debug(1, "Connection %d: RTSP thread exit.", conn->connection_number);
}

void rtsp_listen_loop_cleanup_handler(__attribute__((unused)) void *arg) {
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  debug(2, "rtsp_listen_loop_cleanup_handler called.");
  cancel_all_RTSP_threads(unspecified_stream_category,
                          0); // kill all RTSP listeners
  int *sockfd = (int *)arg;
  mdns_unregister();
  if (sockfd) {
    int i;
    for (i = 1; i <= sockfd[0]; i++) {
      debug(2, "closing socket %d.", sockfd[i]);
      close(sockfd[i]);
    }
    free(sockfd);
  }
  pthread_setcancelstate(oldState, NULL);
}

void *rtsp_listen_loop(__attribute((unused)) void *arg) {
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
  struct addrinfo hints, *info, *p;
  char portstr[6];
  int *sockfd = NULL;
  int nsock = 0;
  int i, ret;

  playing_conn = NULL; // the data structure representing the connection that
                       // has the player.

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  snprintf(portstr, 6, "%d", config.port);

  // debug(1,"listen socket port request is \"%s\".",portstr);

  ret = getaddrinfo(NULL, portstr, &hints, &info);
  if (ret) {
    die("getaddrinfo failed: %s", gai_strerror(ret));
  }

  for (p = info; p; p = p->ai_next) {
    ret = 0;
    int fd = socket(p->ai_family, p->ai_socktype, IPPROTO_TCP);
    int yes = 1;

    // Handle socket open failures if protocol unavailable (or IPV6 not handled)
    if (fd != -1) {
      // Set the RTSP socket to close on exec() of child processes
      // otherwise background run_this_before_play_begins or
      // run_this_after_play_ends commands that are sleeping prevent the daemon
      // from being restarted because the listening RTSP port is still in use.
      // See: https://github.com/mikebrady/shairport-sync/issues/329
      fcntl(fd, F_SETFD, FD_CLOEXEC);
      ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

      struct timeval tv;
      tv.tv_sec = 3; // three seconds write timeout
      tv.tv_usec = 0;
      if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof tv) == -1)
        debug(1, "Error %d setting send timeout for rtsp writeback.", errno);

      if ((config.dont_check_timeout == 0) && (config.timeout != 0)) {
        tv.tv_sec = config.timeout; // 120 seconds read timeout by default.
        tv.tv_usec = 0;
        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv) == -1)
          debug(1, "Error %d setting read timeout for rtsp connection.", errno);
      }
#ifdef IPV6_V6ONLY
      // some systems don't support v4 access on v6 sockets, but some do.
      // since we need to account for two sockets we might as well
      // always.
      if (p->ai_family == AF_INET6) {
        ret |= setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &yes, sizeof(yes));
      }
#endif

      if (!ret)
        ret = bind(fd, p->ai_addr, p->ai_addrlen);

      // one of the address families will fail on some systems that
      // report its availability. do not complain.

      if (ret) {
        char *family;
#ifdef AF_INET6
        if (p->ai_family == AF_INET6) {
          family = "IPv6";
        } else
#endif
          family = "IPv4";
        debug(1, "unable to listen on %s port %d. The error is: \"%s\".", family, config.port,
              strerror(errno));
      } else {
        listen(fd, 255);
        nsock++;
        sockfd = realloc(sockfd, (nsock + 1) * sizeof(int));
        sockfd[nsock] = fd;
        sockfd[0] = nsock; // the first entry is the number of sockets in the array
      }
    }

    /*
        listen(fd, 5);
        nsock++;
        sockfd = realloc(sockfd, nsock * sizeof(int));
        sockfd[nsock - 1] = fd;
    */
  }

  freeaddrinfo(info);

  if (nsock) {
    int maxfd = -1;
    fd_set fds;
    FD_ZERO(&fds);
    // skip the first element in sockfd -- it's the count
    for (i = 1; i <= nsock; i++) {
      if (sockfd[i] > maxfd)
        maxfd = sockfd[i];
    }

    char **t1 = txt_records; // ap1 test records
    char **t2 = NULL;        // possibly two text records

    // make up a secondary set of text records
    t2 = secondary_txt_records; // second set of text records in AirPlay 2 only

    build_bonjour_strings(NULL); // no conn yet
    mdns_register(t1, t2);

    pthread_setcancelstate(oldState, NULL);
    int acceptfd;
    struct timeval tv;
    pthread_cleanup_push(rtsp_listen_loop_cleanup_handler, (void *)sockfd);
    do {
      pthread_testcancel();
      tv.tv_sec = 60;
      tv.tv_usec = 0;

      // skip the first element in sockfd -- it's the count
      for (i = 1; i <= nsock; i++)
        FD_SET(sockfd[i], &fds);

      ret = select(maxfd + 1, &fds, 0, 0, &tv);
      if (ret < 0) {
        if (errno == EINTR)
          continue;
        break;
      }

      cleanup_threads();

      acceptfd = -1;
      // skip the first element in sockfd -- it's the count
      for (i = 1; i <= nsock; i++) {
        if (FD_ISSET(sockfd[i], &fds)) {
          acceptfd = sockfd[i];
          break;
        }
      }
      if (acceptfd < 0) // timeout
        continue;

      rtsp_conn_info *conn = malloc(sizeof(rtsp_conn_info));
      if (conn == 0)
        die("Couldn't allocate memory for an rtsp_conn_info record.");
      memset(conn, 0, sizeof(rtsp_conn_info));
      conn->connection_number = RTSP_connection_index++;

      conn->airplay_type = ap_2;  // changed if an ANNOUNCE is received
      conn->timing_type = ts_ptp; // changed if an ANNOUNCE is received

      // TLH: actual accept of RSTP socket

      socklen_t size_of_reply = sizeof(SOCKADDR);
      conn->fd = accept(acceptfd, (struct sockaddr *)&conn->remote, &size_of_reply);
      if (conn->fd < 0) {
        debug(1, "Connection %d: New connection on port %d not accepted:", conn->connection_number,
              config.port);
        perror("failed to accept connection");
        free(conn);
      } else {
        size_of_reply = sizeof(SOCKADDR);
        if (getsockname(conn->fd, (struct sockaddr *)&conn->local, &size_of_reply) == 0) {
          // initialise the connection info
          void *client_addr = NULL, *self_addr = NULL;
          conn->connection_ip_family = conn->local.SAFAMILY;

#ifdef AF_INET6
          if (conn->connection_ip_family == AF_INET6) {
            struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)&conn->remote;
            client_addr = &(sa6->sin6_addr);
            conn->client_rtsp_port = ntohs(sa6->sin6_port);

            sa6 = (struct sockaddr_in6 *)&conn->local;
            self_addr = &(sa6->sin6_addr);
            conn->self_rtsp_port = ntohs(sa6->sin6_port);
            conn->self_scope_id = sa6->sin6_scope_id;
          }
#endif
          if (conn->connection_ip_family == AF_INET) {
            struct sockaddr_in *sa4 = (struct sockaddr_in *)&conn->remote;
            client_addr = &(sa4->sin_addr);
            conn->client_rtsp_port = ntohs(sa4->sin_port);

            sa4 = (struct sockaddr_in *)&conn->local;
            self_addr = &(sa4->sin_addr);
            conn->self_rtsp_port = ntohs(sa4->sin_port);
          }

          inet_ntop(conn->connection_ip_family, client_addr, conn->client_ip_string,
                    sizeof(conn->client_ip_string));
          inet_ntop(conn->connection_ip_family, self_addr, conn->self_ip_string,
                    sizeof(conn->self_ip_string));

          debug(2, "Connection %d: new connection from %s:%u to self at %s:%u.",
                conn->connection_number, conn->client_ip_string, conn->client_rtsp_port,
                conn->self_ip_string, conn->self_rtsp_port);
        } else {
          debug(1, "Error figuring out Shairport Sync's own IP number.");
        }

        //
        // TLH: start thread for RTSP conversation
        //

        ret = pthread_create(&conn->thread, NULL, rtsp_conversation_thread_func,
                             conn); // also acts as a memory barrier
        if (ret) {
          char errorstring[1024];
          strerror_r(ret, (char *)errorstring, sizeof(errorstring));
          die("Connection %d: cannot create an RTSP conversation thread. Error "
              "%d: \"%s\".",
              conn->connection_number, ret, (char *)errorstring);
        }
        debug(3, "Successfully created RTSP receiver thread %d.", conn->connection_number);
        conn->running = 1; // this must happen before the thread is tracked
        track_thread(conn);
      }
    } while (1);
    pthread_cleanup_pop(1); // should never happen
  } else {
    warn("could not establish a service on port %d -- program terminating. Is "
         "another instance of "
         "Shairport Sync running on this device?",
         config.port);
  }
  debug(1, "Oops -- fell out of the RTSP select loop");
  pthread_exit(NULL);
}