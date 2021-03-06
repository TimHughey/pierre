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
  if (_pcm) {
    snd_pcm_close(_pcm);
    _pcm = nullptr;
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

void AudioTx::audioInThread() {
  fprintf(stderr, "audioInThread running...\n");

  int snd_rc = -1;

  while (true) {
    snd_rc = snd_pcm_wait(_pcm, 100);

    if (snd_rc < 0) {
      if (snd_rc == 0) {
        fprintf(stderr, "PCM capture timeout\n");
      } else {
        recoverStream(snd_rc);
      }
      continue;
    }

    auto frames_ready = snd_pcm_avail_update(_pcm);

    if (frames_ready <= 0) {
      recoverStream(frames_ready);
      continue;
    }

    // frames are ready for capture
    auto bytes_ready = framesToBytes(frames_ready);
    ptrByteBuffer buff(new ByteBuffer_t);

    // to use the vector as a buffer
    buff.get()->resize(bytes_ready);
    buff.get()->assign(bytes_ready, 0x00); // zero the buffer

    auto raw = buff.get()->data();
    auto frames = snd_pcm_mmap_readi(_pcm, raw, frames_ready);

    // audio data was captured
    if (frames > 0) {
      auto bytes_actual = snd_pcm_frames_to_bytes(_pcm, frames);
      if (bytes_actual != bytes_ready) {
        fprintf(stderr, "pcm reported %lu bytes ready but only read %lu\n",
                bytes_ready, bytes_actual);
        buff.get()->resize(bytes_actual);
      }

      pushBuffer(buff);
    } else {
      // frames is the error code
      recoverStream(frames);
    }
  }
}

bool AudioTx::init() {
  auto rc = true;
  const auto stream = SND_PCM_STREAM_CAPTURE;
  int open_mode = 0;

  auto err = snd_pcm_open(&_pcm, pcm_name, stream, open_mode);
  if (err < 0) {
    fprintf(stderr, "audio open error: %s", snd_strerror(err));
    rc = false;
  }

  err = snd_output_stdio_attach(&_pcm_log, stderr, 0);

  udpInit();
  setParams();

  snd_pcm_start(_pcm);

  if (isRunning() == false) {
    fprintf(stderr, "[FATAL] PCM is not running\n");
    rc = false;
  }

  return rc;
}

void AudioTx::netOutThread() {
  fprintf(stderr, "netOutThread running...\n");

  // allocate and zero the initial network buffer
  ptrByteBuffer net_buff(new ByteBuffer_t);
  net_buff.get()->resize(_net_packet_size);
  net_buff.get()->assign(_net_packet_size, 0x00);

  // iterator that always points to position to where the next data copy
  // will begin
  auto net_buff_pos = net_buff.get()->begin();

  while (true) {
    auto data = popBuffer();

    // auto data_pos = data.get()->cbegin();
    // fprintf(stderr, "data buffer size: %lu\n", data.get()->size());

    // copy the data buffer to the net buffer until the net buffer is full
    for (const uint8_t byte : *data.get()) {
      if (net_buff_pos == net_buff.get()->cend()) {
        // net buff is full, send it...
        const auto raw = net_buff.get()->data();
        const auto len = net_buff.get()->size();
        auto sent = sendto(_send_socket, raw, len, 0, _dest_ipv4, _dest_size);

        if (sent != len) {
          fprintf(stderr, "[WARN] sent %lu of %lu bytes\n", sent, len);
        }

        // fprintf(stderr, "sent %ld bytes of %lu\n", tx_bytes, raw_bytes);

        // allocate the next net buffer
        net_buff = ptrByteBuffer(new ByteBuffer_t);
        net_buff.get()->reserve(_net_packet_size);
        net_buff.get()->assign(_net_packet_size, 0x00);

        // set net_buff_pos to the beginning of the new buffer
        net_buff_pos = net_buff.get()->begin();

        // keep copying the data
      }
      // the net_buff isn't full yet, copy the data to it
      *net_buff_pos = byte;

      net_buff_pos++;
    }
  }
}

bool AudioTx::run() {
  audioInThreadStart();
  netOutThreadStart();

  audio_in.join();
  net_out.join();

  return true;
}

bool AudioTx::setParams(void) {
  bool rc = true;
  int err;

  snd_pcm_access_mask_t *mask = nullptr;

  err = snd_pcm_hw_params_any(_pcm, _params);
  if (err < 0) {
    fprintf(stderr, "PCM: no configurations available");
    return false;
  }

  mask = (snd_pcm_access_mask_t *)alloca(snd_pcm_access_mask_sizeof());
  snd_pcm_access_mask_none(mask);
  snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_INTERLEAVED);
  snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
  snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_COMPLEX);
  err = snd_pcm_hw_params_set_access_mask(_pcm, _params, mask);

  if (err < 0) {
    fprintf(stderr, "access type not available");
    return false;
  }

  err = snd_pcm_hw_params_set_format(_pcm, _params, format());
  if (err < 0) {
    fprintf(stderr, "sample format not available");
    return false;
  }

  err = snd_pcm_hw_params_set_channels(_pcm, _params, channels());
  if (err < 0) {
    fprintf(stderr, "channel count not available");
    return false;
  }

  err = snd_pcm_hw_params_set_rate_near(_pcm, _params, &(_config.rate), 0);

  snd_pcm_uframes_t psize_min = 0;
  if (snd_pcm_hw_params_get_period_size_min(_params, &psize_min, 0) >= 0) {
    snd_pcm_hw_params_set_period_size(_pcm, _params, psize_min, 0);
  }

  // reportBufferMin();

  const snd_pcm_uframes_t buff_size = 4096;
  snd_pcm_hw_params_set_buffer_size(_pcm, _params, buff_size);

  _monotonic = snd_pcm_hw_params_is_monotonic(_params);
  _can_pause = snd_pcm_hw_params_can_pause(_params);

  err = snd_pcm_hw_params(_pcm, _params);
  if (err < 0) {
    fprintf(stderr, "unable to install hw params:");
    snd_pcm_hw_params_dump(_params, _pcm_log);
    return false;
  }

  snd_pcm_hw_params_get_periods(_params, &_periods, 0);

  err = snd_pcm_sw_params_current(_pcm, _swparams);
  if (err < 0) {
    fprintf(stderr, "unable to get current sw params.");
    return false;
  }

  err = snd_pcm_sw_params_set_avail_min(_pcm, _swparams, 128);

  constexpr snd_pcm_uframes_t sthres_max = 512;
  const snd_pcm_uframes_t sthres = std::min(buff_size / 2, sthres_max);
  err = snd_pcm_sw_params_set_start_threshold(_pcm, _swparams, sthres);
  assert(err >= 0);

  if (snd_pcm_sw_params(_pcm, _swparams) < 0) {
    fprintf(stderr, "unable to install sw params:");
    snd_pcm_sw_params_dump(_swparams, _pcm_log);
    return false;
  }

  // snd_pcm_dump(_pcm, _pcm_log);

  // bits_per_sample = snd_pcm_format_physical_width(format());
  // significant_bits_per_sample = snd_pcm_format_width(format());
  // bits_per_frame = bits_per_sample * channels();
  chunk_bytes = framesToBytes(_chunk_size);

  /* show mmap buffer arrangment */
  // const snd_pcm_channel_area_t *areas;
  // snd_pcm_uframes_t offset, size = _chunk_size;
  // int i;
  // err = snd_pcm_mmap_begin(_pcm, &areas, &offset, &size);
  // if (err < 0) {
  //   fprintf(stderr, "snd_pcm_mmap_begin problem: %s", snd_strerror(err));
  //   return false;
  // }
  // for (i = 0; i < channels(); i++)
  //   fprintf(stderr, "mmap_area[%i] = %p,%u,%u (%u)\n", i, areas[i].addr,
  //           areas[i].first, areas[i].step,
  //           snd_pcm_format_physical_width(format()));
  // /* not required, but for sure */
  // snd_pcm_mmap_commit(_pcm, offset, 0);

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

  err = snd_pcm_avail_delay(_pcm, &avail, &delay);

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
//   snd_pcm_start(_pcm);
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
//     auto wait_rc = snd_pcm_wait(_pcm, 100);
//
//     if (wait_rc == 1) {
//       snd_pcm_avail_delay(_pcm, &avail, &delay);
//
//       auto sframes_read = snd_pcm_mmap_readi(_pcm, audiobuf + raw_count,
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
//       auto recover_rc = snd_pcm_recover(_pcm, wait_rc, 0);
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
