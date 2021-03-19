/*
    Pierre - Custom Light Show via DMX for Wiss Landing
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

#ifndef _pierre_audio_pcm_hpp
#define _pierre_audio_pcm_hpp

#include <iostream>
#include <limits>
#include <set>
#include <thread>
#include <vector>

#include <alloca.h>
#include <alsa/asoundlib.h>

#include "audio/samples.hpp"

namespace pierre {
namespace audio {

class Pcm {
public:
  struct Config {
    struct {
      snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
      uint channels = 2;
      uint rate = 48000;
      uint avail_min = 128;
      uint start_delay = 1;
      uint stop_delay = 0;
    } pcm;

    struct {
      bool init = false;
    } log;
  };

public:
  Pcm(const Config &cfg);
  ~Pcm();

  Pcm(const Pcm &) = delete;
  Pcm &operator=(const Pcm &) = delete;

  void addProcessor(std::shared_ptr<Samples> processor) {
    _processors.insert(processor);
  }

  std::shared_ptr<std::thread> run();

private:
  auto bytesToFrames(size_t bytes) const {
    auto frames = snd_pcm_bytes_to_frames(_pcm, bytes);
    return frames;
  }

  auto bytesToSamples(size_t bytes) const {
    auto samples = snd_pcm_bytes_to_samples(_pcm, bytes);
    return samples;
  }

  uint32_t channels() const { return _cfg.pcm.channels; }
  snd_pcm_format_t format() const { return _cfg.pcm.format; }

  auto framesToBytes(snd_pcm_sframes_t frames) {
    auto bytes = snd_pcm_frames_to_bytes(_pcm, frames);

    return bytes;
  }
  void init();
  bool isRunning() const {
    auto rc = false;

    if (snd_pcm_state(_pcm) == SND_PCM_STATE_RUNNING) {
      rc = true;
    }

    return rc;
  }
  uint32_t rate() const { return _cfg.pcm.rate; }
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

    std::cerr << "buffer_time_min=" << buffer_time_min << "µs ";
    std::cerr << "buffer_size_min=" << buffer_size_min << std::endl;
  }

  bool setParams();
  void stream();

private:
  const char *pcm_name = "hw:CARD=sndrpihifiberry,DEV=0";
  Config _cfg;
  bool _init_rc = false;

  snd_pcm_t *_pcm = nullptr;
  snd_output_t *_pcm_log;

  snd_pcm_hw_params_t *_params = nullptr;
  snd_pcm_sw_params_t *_swparams = nullptr;
  snd_pcm_uframes_t _chunk_size = 1024;
  uint _periods = 0;
  int _monotonic = 0;
  int _can_pause = 0;

  bool _shutdown = false;

  std::set<spSamples> _processors;
};
} // namespace audio
} // namespace pierre

#endif
