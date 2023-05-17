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

#include "frame/flush_info.hpp"
#include "base/logger.hpp"

#include <cassert>
#include <exception>
#include <set>

namespace pierre {

FlushInfo::FlushInfo(seq_num_t from_seq, ftime_t from_ts, //
                     seq_num_t until_seq, ftime_t until_ts) noexcept
    : kind{Normal},
      // set optional fields
      from_seq(from_seq), from_ts(from_ts),
      // flush everything <= seq_num
      until_seq(until_seq),
      // flush everything <= timestamp
      until_ts(until_ts) {}

FlushInfo::FlushInfo(kind_t want_kind) noexcept : kind{want_kind} {
  switch (want_kind) {
  case All:
  case Inactive:
    // allowed kinds without including flush details
    break;

  default:
    assert("must provide flush info");
  }
}

FlushInfo::lock_t FlushInfo::accept(FlushInfo &&fi) noexcept {
  INFO_AUTO_CAT("accept");
  static const std::set<kind_t> invalid{Inactive, Complete};

  if (invalid.contains(kind)) {
    assert("attempted accepted on invalid kind");
  }

  INFO_AUTO("{}", fi);

  mtx = std::make_unique<mutex_t>();
  a_count = std::make_unique<a_int64_t>(0);

  // protect copying of new flush details
  lock_t l(*mtx, std::defer_lock);
  l.lock();

  // copy the new flush info
  std::swap(from_seq, fi.from_seq);
  std::swap(from_ts, fi.from_ts);
  std::swap(until_seq, fi.until_seq);
  std::swap(until_ts, fi.until_ts);
  std::swap(kind, fi.kind);

  INFO_AUTO("returning lock");

  return l;
}

void FlushInfo::busy_hold() noexcept {
  static const std::set<kind_t> no_hold{Inactive, Complete};

  // return quickly depending on kind
  if (no_hold.contains(kind)) return;

  // prevent collisions
  lock_t l(*mtx, std::defer_lock);
  l.lock();
  l.unlock();
}

void FlushInfo::done(lock_t &l) noexcept {
  INFO_AUTO_CAT("done");

  INFO_AUTO("{}", *this);

  from_seq = from_ts = 0;
  until_seq = until_ts = 0;
  kind = Complete;

  l.unlock();
  INFO_AUTO("unlocked");
}

} // namespace pierre