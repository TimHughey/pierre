// Pierre - Custom Light Show for Wiss Landing
// Copyright (C) 2021  Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#pragma once

#include "base/anchor_last.hpp"
#include "base/input_info.hpp"
#include "base/io.hpp"
#include "base/logger.hpp"
#include "base/pet.hpp"
#include "base/threads.hpp"
#include "base/types.hpp"
#include "frame/flush_info.hpp"
#include "frame/frame.hpp"
#include "frame/reel.hpp"

#include <algorithm>
#include <atomic>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <set>
#include <shared_mutex>
#include <stop_token>

namespace pierre {

using racked_reels = std::map<reel_serial_num_t, Reel>;

class Racked : public std::enable_shared_from_this<Racked> {
public:
  Racked() noexcept
      : guard(io::make_work_guard(io_ctx)), // ensure io_ctx has work
        handoff_strand(io_ctx),             // unprocessed frame 'queue'
        wip_strand(io_ctx),                 // guard work in progress reeel
        frame_strand(io_ctx),               // used for next frame
        flush_strand(io_ctx),               // used to isolate flush (and interrupt other strands)
        wip_timer(io_ctx)                   // used to racked incomplete wip reels
  {}

  static auto ptr() noexcept { return self()->shared_from_this(); }

  static void flush(FlushInfo request);

  // handeff() allows the packet to be moved however expects the key to be a reference
  static void handoff(uint8v packet, const uint8v &key) noexcept {
    auto s = ptr();

    auto frame = Frame::create(packet);

    if (frame->state.header_parsed()) {
      if (s->flush_request.should_flush(frame)) {
        frame->flushed();
      } else if (frame->decipher(std::move(packet), key)) {
        // note:  we move the packet since handoff() is done with it

        asio::post(s->handoff_strand, [=]() { s->accept_frame(frame); });
      }
    }

    if (!frame->deciphered()) {
      INFO(module_id, "HANDOFF", "DISCARDING frame={}\n", frame->inspect());
    }
  }

  static void init();
  const string inspect() const noexcept;

  static frame_future next_frame() noexcept;

private:
  enum log_racked_rc { NONE, RACKED, COLLISION, TIMEOUT };

private:
  void accept_frame(frame_t frame) noexcept;
  void add_frame(frame_t frame) noexcept;

  void flush_impl(FlushInfo request);
  void next_frame_impl(frame_promise prom) noexcept;

  void init_self();

  void monitor_wip() noexcept;

  void rack_wip() noexcept;

  static std::shared_ptr<Racked> &self() noexcept;

  // misc logging, debug
  void log_racked() const noexcept { log_racked(string()); }
  void log_racked(const string &wip_info, log_racked_rc rc = log_racked_rc::NONE) const noexcept;

private:
  // order dependent
  io_context io_ctx;
  work_guard_t guard;
  strand handoff_strand;
  strand wip_strand;
  strand frame_strand;
  strand flush_strand;
  steady_timer wip_timer;

  // order independent
  FlushInfo flush_request;
  std::shared_timed_mutex rack_mtx;
  std::shared_timed_mutex wip_mtx;

  racked_reels racked;
  std::optional<Reel> wip;
  frame_t first_frame;

  // threads
  Threads _threads;
  stop_tokens _stop_tokens;

private:
  static uint64_t REEL_SERIAL_NUM; // ever incrementing, no dups

public:
  static constexpr Nanos reel_max_wait = InputInfo::lead_time_min;
  static constexpr csv module_id{"RACKED"};

}; // Racked

} // namespace pierre