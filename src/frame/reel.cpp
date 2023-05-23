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
#include "base/stats.hpp"
#include "frame/anchor.hpp"
#include "frame/flusher.hpp"
#include "frame/master_clock.hpp"

#include <algorithm>

namespace pierre {

static auto next_sn() noexcept {
  static std::atomic_uint64_t n{0x0001};

  return std::atomic_fetch_add(&n, 1);
}

Reel::kind_desc_t Reel::kind_desc{string("Empty"), string("Ready")};

Reel::Reel(kind_t kind) noexcept
    : kind{kind},        //
      serial(next_sn()), //
      cached_size(std::make_unique<std::atomic_int64_t>(0)) {}

Reel::clean_results Reel::clean() noexcept {
  INFO_AUTO_CAT("clean");

  ssize_t erased{0};
  ssize_t remain{0};

  if (size_cached() > 0) {
    auto l = lock();

    erased = std::erase_if(frames, [](const Frame &f) { return f.dont_render(); });

    if (erased > 0) {
      remain = std::ssize(frames);
      INFO_AUTO("erased={} remain={}", erased, remain);
      cached_size->store(remain);

      record_size();
    }

    l.unlock();
  }

  return std::make_pair(erased, remain);
}

ssize_t Reel::flush(Flusher &flusher) noexcept {
  INFO_AUTO_CAT("flush");
  //// NOTE:  Flusher::check() marks the frame flushed, as needed

  if (empty()) return cached_size->load();

  auto l = lock();

  for (auto &f : frames) {
    flusher.check(f);
  }

  if (frames.front().flushed() && frames.back().flushed()) {
    INFO_AUTO("flushed all frames");
    // all frames flushed, reset reel
    reset(Ready);
  } else {
    std::erase_if(frames, [](const auto &f) {
      auto rc = f.flushed();

      if (rc) INFO_AUTO("{}", f);

      return rc;
    });

    cached_size->store(std::ssize(frames));
    record_size();
  }

  l.unlock();

  return cached_size->load();
}

void Reel::frame_next(AnchorLast ancl, Flusher &flusher, Frame &frame) noexcept {
  INFO_AUTO_CAT("frame_next");

  auto l = lock();

  auto found_it = std::find_if(frames.begin(), frames.end(), [&, this](Frame &f) {
    // first things first, check if this frame is flushed
    // if it is flushed then it's not a live frame, return false
    // we do this here since we're under the protection of flusher.aquire()
    // and to ensure that frames requiring flushing that were added by
    // Reel.push_back() are caught
    if (f.flush(flusher)) return false;

    // not flushed, proceed with calculating state
    f.state_now(ancl);

    if (f.can_render()) return true;

    INFO_AUTO("SKIP {}", f);

    if (f.outdated()) {
      f.record_state();
      f.record_sync_wait();
    }

    return false;
  });

  if (found_it != frames.end()) {
    INFO_AUTO("{}", *found_it);
    frame = std::move(*found_it);
  }

  l.unlock();
}

void Reel::push_back(Frame &&frame) noexcept {
  INFO_AUTO_CAT("push_back");

  frame.record_state();
  INFO_AUTO("sn={:>08x} ts={:08x}", frame.sn(), frame.ts());

  auto l = lock();
  frames.push_back(std::forward<decltype(frame)>(frame));
  l.unlock();

  std::atomic_fetch_add(cached_size.get(), 1);

  record_size();
}

ssize_t Reel::record_size() noexcept {

  Stats::write<Reel>(stats::FRAMES_AVAIL, cached_size->load());

  return cached_size->load();
}

void Reel::reset(kind_t k) noexcept {
  serial = next_sn();
  frames = frame_container();
  cached_size->store(0);
  kind = k;

  record_size();
}

} // namespace pierre