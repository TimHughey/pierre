/*
    pierre.hpp - Audio Transmission
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

#ifndef _pierre_audio_tx_hpp
#define _pierre_audio_tx_hpp

#include <condition_variable> // std::condition_variable
#include <limits>
#include <mutex> // std::mutex, std::unique_lock
#include <queue>
#include <string>
#include <thread> // std::thread
#include <vector>

#include <alloca.h>
#include <alsa/asoundlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

namespace pierre {

typedef class AudioTx AudioTx_t;
typedef std::string string_t;

class AudioTx {
public:
  typedef struct {
    snd_pcm_format_t format;
    unsigned int channels;
    unsigned int rate;
  } Config_t;

  typedef std::thread Thread;
  typedef Thread Thread_t;

  typedef std::vector<uint8_t> ByteBuffer_t;
  typedef std::shared_ptr<ByteBuffer_t> ptrByteBuffer;

  typedef std::queue<ptrByteBuffer> BufferQueue_t;

  typedef std::mutex Mutex_t;
  typedef std::unique_lock<Mutex_t> XLock_t;
  typedef std::lock_guard<Mutex_t> LockGuard_t;

  typedef std::condition_variable PendingBuffer_t;
  typedef std::atomic<bool> BufferQueueTimeout_t;

public:
  AudioTx(const string_t &dest_host) : _dest_host(dest_host) {
    _config.format = SND_PCM_FORMAT_S16_LE;
    _config.channels = 2;
    _config.rate = 48000;

    snd_pcm_hw_params_malloc(&_params);
    snd_pcm_sw_params_malloc(&_swparams);
  };

  ~AudioTx();

  bool init();
  bool run();

private: // threads
  void audioInThread();
  static void audioInThreadStatic(AudioTx_t *atx) { atx->audioInThread(); }
  void audioInThreadStart() { audio_in = Thread(audioInThreadStatic, this); }

  void netOutThread();
  static void netOutThreadStatic(AudioTx_t *atx) { atx->netOutThread(); }
  void netOutThreadStart() { net_out = Thread(netOutThreadStatic, this); }

  auto framesToBytes(snd_pcm_sframes_t frames) {
    auto bytes = snd_pcm_frames_to_bytes(_pcm, frames);

    return bytes;
  }

  bool isRunning() const {
    auto rc = false;

    if (snd_pcm_state(_pcm) == SND_PCM_STATE_RUNNING) {
      rc = true;
    }

    return rc;
  }

  ptrByteBuffer popBuffer() {
    XLock_t lock(_buffer_mutex);
    ptrByteBuffer buffer;

    while (_buffer_q.empty()) {
      _buffer_pending.wait(lock);

      if (_buffer_q.empty()) {
        continue;
      }
    }

    buffer = _buffer_q.front();
    _buffer_q.pop();

    return buffer;
  }

  void pushBuffer(ptrByteBuffer buff) {
    LockGuard_t lock(_buffer_mutex);

    if (_buffer_q.size() == _buffer_q_max_depth) {
      _buffer_q.pop();
      _buffers_discarded++;
    }

    _buffer_q.push(buff);
    _buffer_pending.notify_one();
  }

  bool recoverStream(int snd_rc) {
    bool rc = true;

    auto recover_rc = snd_pcm_recover(_pcm, snd_rc, 0);
    if (recover_rc < 0) {
      char err[256] = {0};
      perror(err);
      fprintf(stderr, "recover_rc: %s\n", err);
      rc = false;

      snd_pcm_reset(_pcm);
    }

    snd_pcm_start(_pcm);

    return rc;
  }

  void reportBufferMin() {
    uint buffer_time_min = 0;
    snd_pcm_uframes_t buffer_size_min;

    snd_pcm_hw_params_get_buffer_time_min(_params, &buffer_time_min, 0);
    snd_pcm_hw_params_get_buffer_size_min(_params, &buffer_size_min);

    fprintf(stderr, "buffer_time_min = %5.2fms  buffer_size_min = %lu\n",
            (float)buffer_time_min / 1000.0f, buffer_size_min);
  }

  bool setParams();
  void testPosition(snd_pcm_uframes_t buffer_frames);
  void udpInit();

  uint32_t channels() const { return _config.channels; }
  snd_pcm_format_t format() const { return _config.format; }
  uint32_t rate() const { return _config.rate; }

private:
  const char *pcm_name = "hiberry";
  Config_t _config;

  snd_pcm_t *_pcm = nullptr;

  snd_pcm_hw_params_t *_params = nullptr;
  snd_pcm_sw_params_t *_swparams = nullptr;

  snd_pcm_uframes_t _chunk_size = 1024;

  uint32_t period_time = 0;
  uint32_t buffer_time = 0;
  snd_pcm_uframes_t period_frames = 0;
  int32_t _avail_min = 64;
  int32_t start_delay = 1;
  int32_t stop_delay = 0;

  uint32_t _periods = 0;
  int32_t _monotonic = 0;
  int32_t _can_pause = 0;
  snd_output_t *_pcm_log;

  struct sockaddr_in _dest = {};
  static constexpr size_t _dest_size = sizeof(_dest);
  struct sockaddr *_dest_ipv4 = (struct sockaddr *)&_dest;
  string_t _dest_host;
  const int _addr_family = AF_INET;
  const int _ip_protocol = IPPROTO_IP;

  string_t _dest_port = "48000";
  int _send_socket = -1;

  size_t significant_bits_per_sample, bits_per_sample, bits_per_frame;
  size_t chunk_bytes;

  Thread_t audio_in;
  Thread_t net_out;

  size_t _net_packet_size = 1024;

  Mutex_t _buffer_mutex;
  BufferQueue_t _buffer_q;
  size_t _buffer_q_max_depth = 10;
  size_t _buffers_discarded = 0;

  PendingBuffer_t _buffer_pending;
};

} // namespace pierre

#endif
