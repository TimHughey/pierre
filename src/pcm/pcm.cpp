
//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2022  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com
//
//  This work based on and inspired by
//  https://github.com/mikebrady/nqptp Copyright (c) 2021--2022 Mike Brady.

#include <chrono>
#include <fmt/format.h>
#include <string_view>

#include "pcm/pcm.hpp"
#include "pcm/rfc3550/header.hpp"

namespace pierre {

using namespace std::chrono_literals;
using namespace std::string_view_literals;

PulseCodeMod::PulseCodeMod(const Opts &opts) : queued(opts.audio_raw), nptp(opts.nptp) {
  thread = std::jthread([this]() { runLoop(); });

  //  give this thread a name
  const auto handle = thread.native_handle();
  pthread_setname_np(handle, "PCM");
}

void PulseCodeMod::runLoop() {
  running = true;

  while (true) {
    size_t frame_bytes = 0;

    auto want_bytes = InputInfo::pcmBufferSize();

    auto request = queued.dataRequest(want_bytes);
    frame_bytes = request.get();

    if (queued.deque(buffer, frame_bytes) == false) {
      // fmt::print("{} deque failed\n", fnName());

      std::this_thread::sleep_for(1ms);
      continue;
    }

    // auto *hdr = pcm::rfc3550_hdr::from(buffer.data());

    // fmt::print("{}  len={:>7}  vsn={:#04x}  seqnum={:>7}  timestamp={:>11}\n", fnName(),
    //            hdr->length(), hdr->version(), hdr->seqNum(), hdr->timestamp());
  }

  auto self = pthread_self();

  std::array<char, 24> thread_name{0};
  pthread_getname_np(self, thread_name.data(), thread_name.size());
  fmt::print("{} work is complete\n", thread_name.data());
}

sPulseCodeMod PulseCodeMod::_instance_; // declaration of singleton instance

} // namespace pierre