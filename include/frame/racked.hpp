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

#include "base/input_info.hpp"
#include "io/io.hpp"
#include "base/pet.hpp"
#include "base/threads.hpp"
#include "base/types.hpp"
#include "frame/anchor_last.hpp"
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
#include <shared_mutex>

namespace pierre {

// forward decls to hide implementation details
class Av;

using racked_reels = std::map<reel_serial_num_t, Reel>;

class Racked : public std::enable_shared_from_this<Racked> {
private:
  Racked() noexcept
      : guard(asio::make_work_guard(io_ctx)), // ensure io_ctx has work
        handoff_strand(io_ctx),               // unprocessed frame 'queue'
        wip_strand(io_ctx),                   // guard work in progress reeel
        frame_strand(io_ctx),                 // used for next frame
        flush_strand(io_ctx),                 // used to isolate flush (and interrupt other strands)
        wip_timer(io_ctx)                     // used to racked incomplete wip reels
  {}

  static auto ptr() noexcept { return self->shared_from_this(); }

public:
  static void flush(FlushInfo request);
  static void flush_all() noexcept { flush(FlushInfo::make_flush_all()); }

  // handeff() allows the packet to be moved however expects the key to be a reference
  static void handoff(uint8v packet, const uint8v &key) noexcept {
    if (self.use_count() < 1) return; // quietly ignore packets when Racked is not ready
    if (packet.empty()) return;       // quietly ignore empty packets

    auto s = ptr(); // get fresh shared_ptr to ourself, we'll move it into the lambda

    auto frame = Frame::create(packet); // create frame, will also be moved in lambda (as needed)

    if (frame->state.header_parsed()) {
      if (s->flush_request.should_flush(frame)) {
        frame->flushed();
      } else if (frame->decipher(std::move(packet), key)) {
        // notes:
        //  1. we post to handoff_strand to control the concurrency of decoding
        //  2. we move the packet since handoff() is done with it
        //  3. we grab a reference to handoff strand since we move s
        //  4. we move the frame since no code beyond this point needs it

        auto &handoff_strand = s->handoff_strand;
        asio::post(handoff_strand, [s = std::move(s), frame = std::move(frame)]() {
          // here we do the decoding of the audio data, if decode succeeds we
          // rack the frame into wip

          if (frame->decode(s->av)) [[likely]] {
            // decode success, rack the frame

            auto &wip_strand = s->wip_strand; // will move s, need reference

            // we post to wip_strand to guard wip reel.  that said, we still use a mutex
            // because of flushing and so next_frame can temporarily interrupt inbound
            // decoded packet spooling.  this is important because once a wip is full it
            // must be spooled into racked reels which is also being read from for next_frame()
            asio::post(wip_strand, [s = std::move(s), frame = std::move(frame)]() mutable {
              std::unique_lock lck(s->wip_mtx, std::defer_lock);
              lck.lock();

              // save shared_ptr dereferences for multiple use variables
              auto &wip = s->wip;

              // create the wip, if needed
              if (wip.has_value() == false) wip.emplace(++REEL_SERIAL_NUM);

              wip->add(frame);

              if (wip->full()) {
                s->rack_wip();
              } else if (std::ssize(*wip) == 1) {
                s->monitor_wip(); // handle incomplete wip reels
              }
            });

          } else {
            frame->state.record_state(); // save the failue to timeseries database
          }
        });
      }
    } else {
      frame->state.record_state();
    }
  }

  static void init() noexcept;

  /// @brief Get a shared_future to the next racked frame
  /// @return shared_future containing the next frame (could be silent)
  static frame_future next_frame() noexcept;

  /// @brief Shutdown Racked
  static void shutdown() noexcept;

  static void spool(bool enable = true) noexcept {
    if (!self) return;

    ptr()->spool_frames.store(enable);
  }

private:
  enum log_racked_rc { NONE, RACKED, COLLISION, TIMEOUT };

private:
  void monitor_wip() noexcept;

  void rack_wip() noexcept;

  // misc logging, debug
  void log_racked(log_racked_rc rc = log_racked_rc::NONE) const noexcept;

private:
  // order dependent
  io_context io_ctx;
  work_guard guard;
  strand handoff_strand;
  strand wip_strand;
  strand frame_strand;
  strand flush_strand;
  steady_timer wip_timer;

  // order independent
  FlushInfo flush_request;
  std::atomic_bool spool_frames{false};
  std::shared_ptr<Av> av;
  std::shared_timed_mutex rack_mtx;
  std::shared_timed_mutex wip_mtx;

  racked_reels racked;
  std::optional<Reel> wip;
  frame_t first_frame;

  // threads
  Threads threads;

private:
  static std::shared_ptr<Racked> self;
  static int64_t REEL_SERIAL_NUM; // ever incrementing, no dups

public:
  static constexpr Nanos reel_max_wait{InputInfo::lead_time_min};
  static constexpr csv module_id{"desk.racked"};

}; // Racked

} // namespace pierre