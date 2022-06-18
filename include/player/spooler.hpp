
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

#pragma once

#include "core/typedefs.hpp"
#include "player/flush_request.hpp"
#include "player/rtp.hpp"
#include "player/spool.hpp"
#include "player/typedefs.hpp"
#include "rtp_time/rtp_time.hpp"

#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <deque>
#include <memory>
#include <ranges>

namespace pierre {
namespace player {

namespace { // anonymous namespace limits scope
namespace asio = boost::asio;
} // namespace

class Spooler;
typedef std::shared_ptr<Spooler> shSpooler;

class Spooler : public std::enable_shared_from_this<Spooler> {
private:
  // a spool represents RTP packets, in ascending order by sequence,
  // possibly with gaps
  typedef std::deque<shSpool> Spools;

private:
  Spooler(asio::io_context &io_ctx, csv id);
  friend class FramesRequest;

public:
  static shSpooler create(asio::io_context &io_ctx, csv id);

public:
  shRTP emplace_back(shRTP frame);
  shSpool emplace_back(shSpool spool);
  bool empty() const { return _spools.empty(); }
  size_t firstSpoolAvailableFrames() const;
  void flush(const FlushRequest &flush);
  size_t frameCount() const;

  shRTP nextFrame(const Nanos lead_ns);

  shRTP queueFrame(shRTP frame);
  shSpool queueSpool(shSpool spool) { return _spools.emplace_back(spool); }

  fra::StatsMap statsCalc();
  const string statsMsg() const;
  void statsAsync();

private:
  shSpool add();
  csv moduleId() const { return csv(_module_id); }

private:
  // order dependent (constructor initialized)
  asio::io_context::strand spooler_strand;

  // order independent
  Spools _spools; // zero or more spools where front = earliest, back = latest
  FlushRequest _flush;
  string _module_id;

  // stats reporting
  int64_t frames_last = 0;
};

} // namespace player
} // namespace pierre