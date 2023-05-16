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

namespace pierre {

static auto next_sn() noexcept {
  static std::atomic_uint64_t n{0x1000};

  return std::atomic_fetch_add(&n, 1);
}

Reel::Reel(kind_t kind) noexcept : kind{kind}, serial(next_sn()) {

  switch (kind) {
  case Starter:
    min_frames = InputInfo::frames(500ms);
    break;

  case Transfer:
    min_frames = 1;
    break;

  case Empty:
    min_frames = 0;

  case Ready:
    break;
  }

  if (kind != Empty) {
    cached_size = std::make_unique<std::atomic<ssize_t>>(0);
    mtx = std::make_unique<std::mutex>();
  }
}

std::pair<ssize_t, ssize_t> Reel::clean() noexcept {
  auto a = frames.begin();
  auto b = frames.end();

  auto to_it = std::find_if(a, b, [](const auto &f) { return f.can_render(); });

  auto e = frames.erase(a, to_it);
  auto r = std::ssize(frames);

  cached_size->store(r);

  return std::make_pair(std::distance(frames.begin(), e), r);
}

void Reel::clear() noexcept {
  cached_size->store(0);

  frames.clear();
}

ssize_t Reel::flush(FlushInfo &fi) noexcept {
  lock_guard lck(*mtx);

  while (fi.check(frames.front())) {
    pop_front();
  }

  return fi.flushed();
}

bool Reel::pop_front() noexcept {
  if (!empty()) {
    frames.pop_front();
    std::atomic_fetch_sub(cached_size.get(), 1);
  }

  return empty();
}

ssize_t Reel::size() const noexcept {
  if (kind == Empty) return 0;

  return std::ssize(frames);
}

void Reel::take(Reel &o) noexcept {
  INFO_AUTO_CAT("take");

  if (serial == o.serial) return;

  if (o.size_cached() >= size_min()) {
    lock_guard lck(*o.mtx);

    INFO_AUTO("taking {}", o);

    serial = std::move(o.serial);
    o.serial = next_sn();

    frames = std::move(o.frames);
    o.frames = frame_container();

    cached_size->store(o.cached_size->load());
    o.cached_size->store(0);

    kind = Ready;
    INFO_AUTO("got {}", *this);
  }
}

void Reel::push_back(Frame &&frame) noexcept {
  INFO_AUTO_CAT("push_back");

  std::atomic_fetch_add(cached_size.get(), 1);

  INFO_AUTO("avail={:>5} {}", cached_size->load(), frame);

  lock_guard lck(*mtx);
  frames.push_back(std::forward<decltype(frame)>(frame));
}

} // namespace pierre