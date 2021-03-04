/*
    pierre.cpp - Audio Transmission
    Copyright (C) 2021  Tim Hughey

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

#include "pierre/audiotx.hpp"

using namespace pierre;

AudioTx::~AudioTx() {
  if (handle) {
    snd_pcm_close(handle);
    handle = nullptr;
  }

  if (audiobuf) {
    free(audiobuf);
    audiobuf = nullptr;
  }

  if (_params) {
    snd_pcm_hw_params_free(_params);
    _params = nullptr;
  }

  if (_swparams) {
    snd_pcm_sw_params_free(_swparams);
    _swparams = nullptr;
  }

  if (_send_socket >= 0) {
    close(_send_socket);
    _send_socket = -1;
  }
}

bool AudioTx::init() {
  auto rc = true;
  const auto stream = SND_PCM_STREAM_CAPTURE;
  int open_mode = 0;

  auto err = snd_pcm_open(&handle, pcm_name, stream, open_mode);
  if (err < 0) {
    fprintf(stderr, "audio open error: %s", snd_strerror(err));
    rc = false;
  }

  err = snd_output_stdio_attach(&log, stderr, 0);

  udpInit();
  setParams();

  return rc;
}

void AudioTx::tsProcess(AudioTx_t *atx) { atx->processThread(); }

AudioTx::Thread_t AudioTx::process() {
  auto proc_thread = std::thread(tsProcess, this);

  return proc_thread;
}

void AudioTx::processThread() {
  snd_pcm_start(handle);

  if (isRunning() == false) {
    fprintf(stderr, "[FATAL] PCM is not running\n");
    return;
  }

  size_t raw_count = 0;
  while (true) {
    snd_pcm_sframes_t avail, delay;

    auto wait_rc = snd_pcm_wait(handle, 100);

    if (wait_rc == 1) {
      snd_pcm_avail_delay(handle, &avail, &delay);

      auto sframes_read = snd_pcm_mmap_readi(handle, audiobuf + raw_count, 128);

      if (sframes_read > 0) {
        raw_count += framesToBytes(sframes_read);

        if (raw_count >= 1024) {
          auto tx_bytes = sendto(_send_socket, audiobuf, raw_count, 0,
                                 _dest_ipv4, sizeof(_dest));

          if (tx_bytes != raw_count) {
            fprintf(stderr, "only sent %ld bytes\n", tx_bytes);
          }

          raw_count = 0;
        }
      }

      // fprintf(stderr, "bytes_avail[%lu] bytes_read[%ld] delay_bytes[%lu]\n",
      //         framesToBytes(avail), framesToBytes(sframes_read),
      //         framesToBytes(delay));
    } else {
      auto recover_rc = snd_pcm_recover(handle, wait_rc, 0);
      if (recover_rc < 0) {
        char err[256] = {0};
        perror(err);
        fprintf(stderr, "recover_rc: %s\n", err);
      }
    }
  }
}

bool AudioTx::run() {
  Thread_t process_thread = process();

  process_thread.join();

  return true;
}

bool AudioTx::setParams(void) {
  bool rc = true;
  int err;

  snd_pcm_access_mask_t *mask = nullptr;

  err = snd_pcm_hw_params_any(handle, _params);
  if (err < 0) {
    fprintf(stderr, "PCM: no configurations available");
    return false;
  }

  mask = (snd_pcm_access_mask_t *)alloca(snd_pcm_access_mask_sizeof());
  snd_pcm_access_mask_none(mask);
  snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_INTERLEAVED);
  snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
  snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_COMPLEX);
  err = snd_pcm_hw_params_set_access_mask(handle, _params, mask);

  if (err < 0) {
    fprintf(stderr, "access type not available");
    return false;
  }

  err = snd_pcm_hw_params_set_format(handle, _params, format());
  if (err < 0) {
    fprintf(stderr, "sample format not available");
    return false;
  }

  err = snd_pcm_hw_params_set_channels(handle, _params, channels());
  if (err < 0) {
    fprintf(stderr, "channel count not available");
    return false;
  }

  err = snd_pcm_hw_params_set_rate_near(handle, _params, &(_config.rate), 0);

  // reportBufferMin();

  const snd_pcm_uframes_t buff_size = 1024;
  snd_pcm_hw_params_set_buffer_size(handle, _params, buff_size);

  _monotonic = snd_pcm_hw_params_is_monotonic(_params);
  _can_pause = snd_pcm_hw_params_can_pause(_params);

  err = snd_pcm_hw_params(handle, _params);
  if (err < 0) {
    fprintf(stderr, "unable to install hw params:");
    snd_pcm_hw_params_dump(_params, log);
    return false;
  }

  snd_pcm_hw_params_get_periods(_params, &_periods, 0);

  err = snd_pcm_sw_params_current(handle, _swparams);
  if (err < 0) {
    fprintf(stderr, "unable to get current sw params.");
    return false;
  }

  err = snd_pcm_sw_params_set_avail_min(handle, _swparams, 128);

  const snd_pcm_uframes_t sthres = buff_size;
  err = snd_pcm_sw_params_set_start_threshold(handle, _swparams, sthres);
  assert(err >= 0);

  if (snd_pcm_sw_params(handle, _swparams) < 0) {
    fprintf(stderr, "unable to install sw params:");
    snd_pcm_sw_params_dump(_swparams, log);
    return false;
  }

  snd_pcm_dump(handle, log);

  // bits_per_sample = snd_pcm_format_physical_width(format());
  // significant_bits_per_sample = snd_pcm_format_width(format());
  // bits_per_frame = bits_per_sample * channels();
  chunk_bytes = framesToBytes(_chunk_size);
  audiobuf = (u_char *)realloc(audiobuf, chunk_bytes);
  if (audiobuf == NULL) {
    fprintf(stderr, "not enough memory");
    return false;
  }

  /* show mmap buffer arrangment */
  // const snd_pcm_channel_area_t *areas;
  // snd_pcm_uframes_t offset, size = _chunk_size;
  // int i;
  // err = snd_pcm_mmap_begin(handle, &areas, &offset, &size);
  // if (err < 0) {
  //   fprintf(stderr, "snd_pcm_mmap_begin problem: %s", snd_strerror(err));
  //   return false;
  // }
  // for (i = 0; i < channels(); i++)
  //   fprintf(stderr, "mmap_area[%i] = %p,%u,%u (%u)\n", i, areas[i].addr,
  //           areas[i].first, areas[i].step,
  //           snd_pcm_format_physical_width(format()));
  // /* not required, but for sure */
  // snd_pcm_mmap_commit(handle, offset, 0);

  testPosition(buff_size);

  return rc;
}

void AudioTx::testPosition(snd_pcm_uframes_t buffer_frames) {
  constexpr int coef = 8;
  static long counter = 0;
  static time_t tmr = -1;
  time_t now;
  static float availsum, delaysum, samples;
  static snd_pcm_sframes_t maxavail, maxdelay;
  static snd_pcm_sframes_t minavail, mindelay;
  static snd_pcm_sframes_t badavail = 0, baddelay = 0;
  snd_pcm_sframes_t outofrange;
  snd_pcm_sframes_t avail, delay;
  int err;

  err = snd_pcm_avail_delay(handle, &avail, &delay);

  if (avail || delay || err) {
    fprintf(stderr, "err[%i] avail[%lu] avail_delay[%lu]\n", err, avail, delay);
  }

  if (err < 0) {
    return;
  }

  outofrange = (coef * (snd_pcm_sframes_t)buffer_frames) / 2;
  if (avail > outofrange || avail < -outofrange || delay > outofrange ||
      delay < -outofrange) {
    badavail = avail;
    baddelay = delay;
    availsum = delaysum = samples = 0;
    maxavail = maxdelay = 0;
    minavail = mindelay = buffer_frames * 16;
    fprintf(stderr,
            "Suspicious buffer position (%li total): "
            "avail = %li, delay = %li, buffer = %li\n",
            ++counter, (long)avail, (long)delay, (long)buffer_frames);
  }

  time(&now);
  if (tmr == (time_t)-1) {
    tmr = now;
    availsum = delaysum = samples = 0;
    maxavail = maxdelay = 0;
    minavail = mindelay = buffer_frames * 16;
  }
  if (avail > maxavail)
    maxavail = avail;

  if (delay > maxdelay)
    maxdelay = delay;

  if (avail < minavail)
    minavail = avail;

  if (delay < mindelay)
    mindelay = delay;

  availsum += avail;
  delaysum += delay;
  samples++;
  if (avail != 0 && now != tmr) {
    fprintf(stderr,
            "BUFPOS: avg%li/%li "
            "min%li/%li max%li/%li (%li) (%li:%li/%li)\n",
            long(availsum / samples), long(delaysum / samples), minavail,
            mindelay, maxavail, maxdelay, buffer_frames, counter, badavail,
            baddelay);
    tmr = now;
  }
}

void AudioTx::udpInit() {

  // _dest_ipv4->sin_family = AF_INET;
  // _dest_ipv4->sin_port = htons(_dest_port);

  _send_socket = socket(_addr_family, SOCK_DGRAM, _ip_protocol);

  struct addrinfo *result = nullptr;

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
  hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
  hints.ai_flags = AI_PASSIVE;
  hints.ai_protocol = 0; /* Any protocol */

  auto rc =
      getaddrinfo(_dest_host.c_str(), _dest_port.c_str(), &hints, &result);
  if (rc != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
    return;
  }
  auto search = result;
  char addrstr[100];

  while (search) {
    struct sockaddr_in *found = nullptr;

    switch (search->ai_family) {
    case AF_INET:
      found = (struct sockaddr_in *)search->ai_addr;
      inet_ntop(search->ai_family, &found->sin_addr, addrstr, 100);

      memcpy(&_dest, found, search->ai_addrlen);

      fprintf(stderr, "sending audio to: %s (%s)\n", _dest_host.c_str(),
              addrstr);
      search = nullptr; // break out of loop, got our address

      break;

    default:
      search = search->ai_next;
      break;
    }
  }
  freeaddrinfo(result);
}

// bool AudioTx::process() {
//   auto rc = true;
//
//   snd_pcm_start(handle);
//
//   if (isRunning() == false) {
//     fprintf(stderr, "[FATAL] PCM is not running\n");
//     return false;
//   }
//
//   size_t raw_count = 0;
//   while (true) {
//     snd_pcm_sframes_t avail, delay;
//
//     auto wait_rc = snd_pcm_wait(handle, 100);
//
//     if (wait_rc == 1) {
//       snd_pcm_avail_delay(handle, &avail, &delay);
//
//       auto sframes_read = snd_pcm_mmap_readi(handle, audiobuf + raw_count,
//       128);
//
//       if (sframes_read > 0) {
//         raw_count += framesToBytes(sframes_read);
//
//         if (raw_count >= 1024) {
//           auto tx_bytes = sendto(_send_socket, audiobuf, raw_count, 0,
//                                  _dest_ipv4, sizeof(_dest));
//
//           if (tx_bytes != raw_count) {
//             fprintf(stderr, "only sent %ld bytes\n", tx_bytes);
//           }
//
//           raw_count = 0;
//         }
//       }
//
//       // fprintf(stderr, "bytes_avail[%lu] bytes_read[%ld]
//       delay_bytes[%lu]\n",
//       //         framesToBytes(avail), framesToBytes(sframes_read),
//       //         framesToBytes(delay));
//     } else {
//       auto recover_rc = snd_pcm_recover(handle, wait_rc, 0);
//       if (recover_rc < 0) {
//         char err[256] = {0};
//         perror(err);
//         fprintf(stderr, "recover_rc: %s\n", err);
//         abort();
//       }
//     }
//   }
//
//   return rc;
// }
