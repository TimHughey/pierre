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
  static std::atomic_uint64_t n{0x0001};

  return std::atomic_fetch_add(&n, 1);
}

Reel::kind_desc_t Reel::kind_desc{string("Empty"), string("Starter"), string("Transfer"),
                                  string("Ready")};

Reel::min_sizes_t Reel::min_sizes{0, InputInfo::frames(250ms), 1, 1};

Reel::Reel(kind_t kind) noexcept
    : kind{kind},        //
      serial(next_sn()), //
      cached_size(std::make_unique<std::atomic_int64_t>(0)) {}

Reel::clean_results Reel::clean() noexcept {
  INFO_AUTO_CAT("clean");

  ssize_t erased{0};
  ssize_t remain{0};

  if (size_cached() > 0) {
    erased = std::erase_if(frames, [](const Frame &f) { return f.dont_render(); });

    if (erased > 0) {
      remain = std::ssize(frames);
      INFO_AUTO("erased={} remain={}", erased, remain);
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
  INFO_AUTO_CAT("flush");
  //// NOTE:  Flusher::check() marks the frame flushed, as needed

  if (empty()) return cached_size->load();

  for (auto &f : frames) {
    flusher.check(f);
  }

  if (frames.front().flushed() && frames.back().flushed()) {
    INFO_AUTO("flushed all frames");
    // all frames flushed, reseet reel
    reset(Starter);
  } else {
    std::erase_if(frames, [](const auto &f) {
      auto rc = f.flushed();

      INFO_AUTO("flushing {}", f);

      return rc;
    });

    cached_size->store(std::ssize(frames));
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

void Reel::reset(kind_t k) noexcept {
  serial = next_sn();
  frames = frame_container();
  cached_size->store(0);
  kind = k;
}

void Reel::transfer(Reel &lhs) noexcept {
  INFO_AUTO_CAT("transfer");

  if (serial == lhs.serial) {
    const auto m = fmt::format("attempt to transfer self serial={}", lhs.serial_num<string>());
    assert(m.c_str());
  }

  // when both a and b reels are empty fallback to wanting a Starter Reel
  if (empty() && lhs.empty()) {
    kind = Starter;
    lhs.kind = Transfer;
  } else if (empty()) {
    // just reel b is empty, set to Transfer so we get at least one frame
    kind = Transfer;

    if (lhs == Reel::Starter) INFO_AUTO("B {}", *this);

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
  frame.record_state();
  frames.push_back(std::forward<decltype(frame)>(frame));

  std::atomic_fetch_add(cached_size.get(), 1);
}

void Reel::push_back(Reel &o_reel) noexcept {
  INFO_AUTO_CAT("push_back");

  if (o_reel.empty()) return;

  INFO_AUTO("A {}", o_reel);

  std::for_each(o_reel.frames.begin(), o_reel.frames.end(),
                [this](auto &f) { frames.push_back(std::move(f)); });

  cached_size->store(std::ssize(frames));

  INFO_AUTO("B {}", *this);

  o_reel.reset();
}

} // namespace pierre