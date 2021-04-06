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

#include "audio/pcm.hpp"
#include "core/state.hpp"

namespace pierre {
namespace audio {

using namespace std;
using std::endl;
using std::make_shared;
using std::shared_ptr;
using std::vector;

Pcm::Pcm(Config &cfg)
    : _alsa_cfg(cfg.pcm("alsa"sv)), _log_cfg(cfg.pcm("logging"sv)) {
  snd_pcm_hw_params_malloc(&_params);
  snd_pcm_sw_params_malloc(&_swparams);
}

Pcm::~Pcm() {
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
}

void Pcm::addProcessor(std::shared_ptr<Samples> processor) {
  _processors.insert(processor);
}

uint32_t Pcm::availMin() const {
  return _alsa_cfg->get("avail_min")->value_or(128);
}

auto Pcm::bytesToFrames(size_t bytes) const {
  auto frames = snd_pcm_bytes_to_frames(_pcm, bytes);
  return frames;
}

auto Pcm::bytesToSamples(size_t bytes) const {
  auto samples = snd_pcm_bytes_to_samples(_pcm, bytes);
  return samples;
}

uint32_t Pcm::channels() const {
  return _alsa_cfg->get("channels"sv)->value_or<uint32_t>(2);
}

snd_pcm_format_t Pcm::format() const {
  string_view str = _alsa_cfg->get("format"sv)->value_or("S16_LE");

  if (str.compare("S16_LE") == 0) {
    return SND_PCM_FORMAT_S16_LE;
  }

  return SND_PCM_FORMAT_S16_LE;
}

auto Pcm::framesToBytes(snd_pcm_sframes_t frames) const {
  auto bytes = snd_pcm_frames_to_bytes(_pcm, frames);

  return bytes;
}

void Pcm::init() {
  const auto stream = SND_PCM_STREAM_CAPTURE;
  int open_mode = 0;

  auto err = snd_output_stdio_attach(&_pcm_log, stderr, 0);

  if (err < 0) {
    goto fatal;
  }

  err = snd_pcm_open(
      &_pcm,
      _alsa_cfg->get("device")->value_or("hw:CARD=sndrpihifiberry,DEV=0"),
      stream, open_mode);
  if (err < 0) {
    cerr << "audio open error: " << snd_strerror(err);
  }

  setParams();

  snd_pcm_start(_pcm);

  if (isRunning() == false) {
    cerr << "[FATAL] PCM is not in running state";
    goto fatal;
  }

  _init_rc = true;

fatal:
  return;
}

bool Pcm::isRunning() const {
  auto rc = false;

  if (snd_pcm_state(_pcm) == SND_PCM_STATE_RUNNING) {
    rc = true;
  }

  return rc;
}

uint32_t Pcm::rate() const { return _alsa_cfg->get("rate")->value_or(48000); }

bool Pcm::recoverStream(int snd_rc) {
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

void Pcm::reportBufferMin() {
  uint buffer_time_min = 0;
  snd_pcm_uframes_t buffer_size_min;

  snd_pcm_hw_params_get_buffer_time_min(_params, &buffer_time_min, 0);
  snd_pcm_hw_params_get_buffer_size_min(_params, &buffer_size_min);

  std::cerr << "buffer_time_min=" << buffer_time_min << "µs ";
  std::cerr << "buffer_size_min=" << buffer_size_min << std::endl;
}

shared_ptr<thread> Pcm::run() {
  auto t = make_shared<thread>([this]() {
    init();

    if (_init_rc == false) {
      cerr << "Pcm:run() initialization failed" << endl;
      return;
    }

    stream();
  });

  return t;
}

bool Pcm::setParams(void) {
  bool rc = true;
  int err;

  snd_pcm_access_mask_t *mask = nullptr;

  err = snd_pcm_hw_params_any(_pcm, _params);
  if (err < 0) {
    cerr << "PCM: no configurations available";
    return false;
  }

  mask = (snd_pcm_access_mask_t *)alloca(snd_pcm_access_mask_sizeof());
  snd_pcm_access_mask_none(mask);
  snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_INTERLEAVED);
  snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
  snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_COMPLEX);
  err = snd_pcm_hw_params_set_access_mask(_pcm, _params, mask);

  if (err < 0) {
    cerr << "access type not available";
    return false;
  }

  err = snd_pcm_hw_params_set_format(_pcm, _params, format());
  if (err < 0) {
    cerr << "sample format not available";
    return false;
  }

  err = snd_pcm_hw_params_set_channels(_pcm, _params, channels());
  if (err < 0) {
    cerr << "channel count not available";
    return false;
  }

  auto sample_rate = rate();
  err = snd_pcm_hw_params_set_rate_near(_pcm, _params, &sample_rate, 0);

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
    cerr << "unable to install hw params:";
    snd_pcm_hw_params_dump(_params, _pcm_log);
    return false;
  }

  snd_pcm_hw_params_get_periods(_params, &_periods, 0);

  err = snd_pcm_sw_params_current(_pcm, _swparams);
  if (err < 0) {
    cerr << "unable to get current sw params.";
    return false;
  }

  err = snd_pcm_sw_params_set_avail_min(_pcm, _swparams, availMin());

  constexpr snd_pcm_uframes_t sthres_max = 512;
  const snd_pcm_uframes_t sthres = std::min(buff_size / 2, sthres_max);
  err = snd_pcm_sw_params_set_start_threshold(_pcm, _swparams, sthres);
  assert(err >= 0);

  if (snd_pcm_sw_params(_pcm, _swparams) < 0) {
    cerr << "unable to install sw params:";
    snd_pcm_sw_params_dump(_swparams, _pcm_log);
    return false;
  }

  if (_log_cfg->get("init")->value_or(false)) {
    snd_pcm_dump(_pcm, _pcm_log);
  }

  return rc;
}

void Pcm::stream() {
  int snd_rc = -1;

  while (core::State::running()) {
    snd_rc = snd_pcm_wait(_pcm, 100);

    if (snd_rc < 0) {
      if (snd_rc == 0) {
        cerr << "Pcm::stream() capture timeout\n";
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

    auto bytes_ready = framesToBytes(frames_ready);
    auto samples_ready = bytesToSamples(bytes_ready);

    auto packet = RawPacket::make_shared(frames_ready, samples_ready);

    // read pcm bytes directly into packet
    auto raw = packet->raw.data();
    auto frames_read = snd_pcm_mmap_readi(_pcm, raw, frames_ready);

    packet->frames = frames_read;

    // send to processors
    for (auto p : _processors) {
      p->push(packet);
    }
  }
}
} // namespace audio
} // namespace pierre
