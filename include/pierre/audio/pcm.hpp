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
  Pcm();
  ~Pcm();

  Pcm(const Pcm &) = delete;
  Pcm &operator=(const Pcm &) = delete;

  void addProcessor(std::shared_ptr<Samples> processor);

  std::shared_ptr<std::thread> run();

private:
  uint32_t availMin() const;
  auto bytesToFrames(size_t bytes) const;
  auto bytesToSamples(size_t bytes) const;
  uint32_t channels() const;
  snd_pcm_format_t format() const;

  auto framesToBytes(snd_pcm_sframes_t frames) const;
  void init();
  bool isRunning() const;
  uint32_t rate() const;
  bool recoverStream(int snd_rc);

  void reportBufferMin();

  bool setParams();
  void stream();

private:
  bool _init_rc = false;

  snd_pcm_t *_pcm = nullptr;
  snd_output_t *_pcm_log;

  snd_pcm_hw_params_t *_params = nullptr;
  snd_pcm_sw_params_t *_swparams = nullptr;
  uint _periods = 0;
  int _monotonic = 0;
  int _can_pause = 0;

  std::set<spSamples> _processors;
};
} // namespace audio
} // namespace pierre

#endif
