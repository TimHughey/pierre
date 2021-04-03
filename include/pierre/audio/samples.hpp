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

#ifndef _pierre_audio_samples_processor_hpp
#define _pierre_audio_samples_processor_hpp

#include <memory>
#include <vector>

#include "misc/mqx.hpp"

namespace pierre {
namespace audio {

typedef std::vector<int16_t> Raw;

class RawPacket {
public:
  RawPacket() = delete;

  RawPacket(const RawPacket &) = delete;
  RawPacket &operator=(const RawPacket &) = delete;

  RawPacket(const long &frames, const long &samples)
      : frames(frames), samples(samples) {

    raw.assign(samples, 0);
  }

  Raw raw;
  size_t bytes; // deprecated
  long frames;
  long samples;

  static std::shared_ptr<RawPacket> make_shared(const long &frames,
                                                const long &samples) {
    auto rp = std::make_shared<RawPacket>(frames, samples);

    return std::move(rp);
  }
};

typedef std::shared_ptr<RawPacket> spRawPacket;

class Samples {
public:
  Samples() { _queue.maxDepth(10); };

  Samples(const Samples &) = delete;
  Samples &operator=(const Samples &) = delete;

  void push(spRawPacket packet) { _queue.push(packet); }

protected:
  spRawPacket pop() {
    auto entry = _queue.pop();

    return entry;
  }

  MsgQX<spRawPacket> _queue;
};

typedef std::shared_ptr<Samples> spSamples;

} // namespace audio
} // namespace pierre

#endif
