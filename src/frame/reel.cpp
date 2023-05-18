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

#include "frame/reel.hpp"
#include "base/logger.hpp"
#include "frame/anchor.hpp"
#include "frame/master_clock.hpp"

#include <atomic>
#include <cassert>

namespace pierre {

static auto next_sn() noexcept {
  static std::atomic_uint64_t n{0x1000};

  return std::atomic_fetch_add(&n, 1);
}

Reel::kind_desc_t Reel::kind_desc{string("Empty"), string("Starter"), string("Transfer"),
                                  string("Ready")};

Reel::min_sizes_t Reel::min_sizes{0, InputInfo::frames(500ms), 1, 1};

Reel::Reel(kind_t kind) noexcept
    : kind{kind},        //
      serial(next_sn()), //
      cached_size(std::make_unique<std::atomic_int64_t>(0)) {}

Reel::clean_results Reel::clean() noexcept {
  ptrdiff_t erased{0};
  ptrdiff_t remain{0};

  if (size_cached() > 0) {
    auto a = frames.begin();
    auto b = frames.end();

    auto to_it = std::find_if(a, b, [](const auto &f) { return f.can_render(); });

    // if we found a renderable frame then erase everything up to that point
    // and update the reel state
    if (to_it != b) {
      auto end_it = frames.erase(a, to_it);
      remain = std::ssize(frames);
      erased = std::distance(frames.begin(), end_it);

      if (remain == 0) kind = Empty;
      cached_size->store(remain);
    }
  }

  return std::make_pair(erased, remain);
}

void Reel::clear() noexcept {
  if (!empty()) {
    cached_size->store(0);
    frames.clear();
  }
}

ssize_t Reel::flush(Flusher &flusher) noexcept {
  //// NOTE:  Flusher::check() marks the frame flushed, as needed

  while (cached_size->load()) {
    auto &frame = frames.front();

    if (flusher.check(frame)) {
      frames.pop_front();
      std::atomic_fetch_sub(cached_size.get(), 1);
    }
  }

  return cached_size->load();
}

bool Reel::pop_front() noexcept {
  if (size_cached() > 0) {
    frames.pop_front();
    std::atomic_fetch_sub(cached_size.get(), 1);
  }

  return cached_size->load() == 0;
}

void Reel::reset() noexcept {
  serial = next_sn();
  frames = frame_container();
  cached_size->store(0);
  kind = Transfer;
}

void Reel::transfer(Reel &lhs) noexcept {
  INFO_AUTO_CAT("take");

  if (serial == lhs.serial) {
    const auto m = fmt::format("attempt to take self serial={}", lhs.serial_num<string>());
    assert(m.c_str());
  }

  // only take reel_a (lhs) when it contains more frames
  // than reel_b (this) has requested
  if (empty()) {

    INFO_AUTO("B {}", *this);

    if (lhs.size_cached() >= size_min()) {

      INFO_AUTO("A {}", lhs);

      serial = std::move(lhs.serial);
      frames = std::move(lhs.frames);
      kind = Ready;

      auto frame_count = lhs.cached_size->load();
      cached_size->store(frame_count);

      lhs.reset();

      INFO_AUTO("B {}", *this);
    }
  }
}

void Reel::push_back(Frame &&frame) noexcept {
  INFO_AUTO_CAT("push_back");

  frame.record_state();
  frames.push_back(std::forward<decltype(frame)>(frame));

  std::atomic_fetch_add(cached_size.get(), 1);
  INFO_AUTO("avail={:>5} {}", cached_size->load(), frame);
}

} // namespace pierre