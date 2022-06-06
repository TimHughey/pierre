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

#include "player/queued.hpp"
#include "player/rtp.hpp"
#include "player/typedefs.hpp"
#include "rtp_time/anchor.hpp"
#include "rtp_time/clock.hpp"

#include <algorithm>
#include <boost/asio.hpp>
#include <chrono>
#include <fmt/format.h>
#include <iterator>
#include <mutex>
#include <optional>
#include <ranges>

using namespace std::chrono_literals;
using steady_clock = std::chrono::steady_clock;

namespace pierre {

namespace shared {
std::optional<player::shQueued> __queued;
std::optional<player::shQueued> &queued() { return __queued; }
} // namespace shared

namespace player {

using error_code = boost::system::error_code;
namespace asio = boost::asio;
namespace errc = boost::system::errc;
namespace ranges = std::ranges;

Queued::Queued(asio::io_context &io_ctx)
    : io_ctx(io_ctx),        // keep a reference to the general io_ctx
      local_strand(io_ctx),  // use this strand for all container actions
      decode_strand(io_ctx), // use this strand for all RTP decoding
      stats_timer(io_ctx) {
  // can not call member functions that use shared_from_this() in constructor
}

// static methods for creating, getting and resetting the shared instance
shQueued Queued::init(asio::io_context &io_ctx) {
  return shared::queued().emplace(new Queued(io_ctx));
}

shQueued Queued::ptr() { return shared::queued().value()->shared_from_this(); }
void Queued::reset() { shared::queued().reset(); }

// general member functions

void Queued::accept(Basic &&packet) {
  shRTP rtp_packet = RTP::create(packet);
  uint32_t seq_num = rtp_packet->seq_num;

  rtp_packet->dump(false);

  if (false) { // debug
    constexpr auto f = FMT_STRING("{} {} "
                                  "now={} sampletime={} ns_since_sample={}\n");

    auto clock_info = MasterClock::ptr()->getInfo();
    const auto now = MasterClock::now();
    const auto time_since_sample = now - clock_info.sampleTime;

    fmt::print(f, runTicks(), fnName(), now.count(), clock_info.sampleTime.count(),
               time_since_sample.count());
  }

  if (rtp_packet->keep(_flush)) {
    // 1. NOT flushed
    // 2. decipher OK

    MasterClock::ptr()->getInfo(); // testing purposes

    // ensure there's a spool available
    if (_spools.empty()) {
      _spools.emplace_back(Spool());
    }

    // back always contains the latest spool
    auto &spool = _spools.back();

    // is there a gap in seq_num?
    if (!spool.empty() && ((spool.back()->seq_num + 1) != seq_num)) {
      // either a gap or rollover, create a new spool
      spool = _spools.emplace_back(Spool());
    }

    // store the rtp_packet in the appropriate spool
    spool.emplace_back(rtp_packet);

    // async decode
    asio::post(decode_strand, // serialize decodes due to single av ctx
               [rtp_packet = rtp_packet, &io_ctx = io_ctx]() mutable {
                 RTP::decode(rtp_packet); // OK to run on separate thread, shared_ptr

                 // find peaks using any available thread
                 asio::post(io_ctx, [rtp_packet = rtp_packet]() mutable {
                   RTP::findPeaks(rtp_packet);
                   rtp_packet->cleanup();
                 });
               });
  }
}

void Queued::flush(const FlushRequest &flush) {
  auto work = [self = shared_from_this(), flush = flush]() mutable {
    bool flush_stats = true;      // debug, output flush stats
    auto &spools = self->_spools; // save typing below

    if (spools.empty()) { // nothing to flush
      return;
    }

    // grab how many packets we currently have for debug stats (below)
    int64_t count_before = 0;
    if (flush_stats) {
      count_before = self->packetCount();
    }

    // create new spools containing the desired seq_nums
    Spools new_spools;
    bool flush_found = false;

    // transfer un-flushed rtp_packets to new spools
    ranges::for_each(spools, [&](Spool &spool) {
      auto fseq_num = flush.until_seq;
      auto fts = flush.until_ts;

      // spool contains flush request
      if ((fseq_num > spool.front()->seq_num) && (fseq_num <= spool.back()->seq_num)) {
        if (true) { // debug
          constexpr auto f = FMT_STRING("{} {} FOUND FLUSH seq={:<7} front={:<7} back={}\n");
          fmt::print(f, runTicks(), moduleId, fseq_num, spool.front()->seq_num,
                     spool.back()->seq_num);
        }

        flush_found = true;
        // the flush seq_num is in this spool. iterate the rtp packets
        // swapping over the packets to retain
        Spool new_spool;
        ranges::for_each(spool, [&](shRTP &rtp) mutable {
          // this is a rtp to keep, swap it into the new spool
          if (rtp->seq_num > flush.until_seq) {
            new_spool.emplace_back(rtp->shared_from_this());
          }
        }); // end of rtp_packet swapping

        // if anything ended up in this new spool then store it
        if (!new_spool.empty()) {
          new_spools.emplace_back(new_spool);
        }
      } else if (fts < spool.front()->timestamp) {
        // spool is newer than flush, keep it
        auto &new_spool = new_spools.emplace_back(Spool());
        std::swap(new_spool, spool);
      } else if (true) { // debug
        constexpr auto f = FMT_STRING("{} {} discarding spool seq_a/b={}/{} count={}\n");
        fmt::print(f, runTicks(), moduleId, spool.front()->seq_num, spool.back()->seq_num,
                   spool.size());
      }
    }); // end of spools processing

    if (!flush_found) {
      // we're dealing with a flush request for a seq_num we haven't seen yet
      // save the flush request
      if (self->_flush.active) { // debug
        if (true) {              // debug
          constexpr auto f = FMT_STRING("{} {} replaced active flush request\n");
          fmt::print(f, runTicks(), moduleId);
        }
      }

      self->_flush = flush; // save the flush request
    }

    // swap new_spools with spools to effectuate the flush
    // note: new spools could be empty if the flush request was for a
    // sequence we've not seen yet
    std::swap(new_spools, spools);

    if (flush_stats) { // debug
      if (self->_flush.active) {
        constexpr auto f = FMT_STRING("{} {} FLUSH active=TRUE until={:<7}\n");
        fmt::print(f, runTicks(), moduleId, flush.until_seq);
      } else {
        auto flushed_count = count_before - self->packetCount();
        constexpr auto f = FMT_STRING("{} {} FLUSH active=FALSE until={:<7} count={}\n");
        fmt::print(f, runTicks(), moduleId, flush.until_seq, flushed_count);
      }
    }
  };

  asio::post(local_strand, work); // schedule the flush
}

void Queued::handoff(const size_t rx_bytes) {
  static bool __stats_active_flag = false;
  if (_packet.empty() || (rx_bytes == 0)) { // no empty packets
    return;
  }

  // ensure stats reporting is active
  if (!__stats_active_flag) {
    if (false) {
      constexpr auto f = FMT_STRING("{} {} stats reporting scheduled\n");
      fmt::print(f, runTicks(), moduleId);
    }

    stats();
    __stats_active_flag = true;
  }

  Basic packet;
  std::swap(packet, _packet); // grab latest packet and reset for the next

  asio::post(       // async process the rtp packet
      local_strand, // local strand guards container
      [self = shared_from_this(), packet = std::move(packet)]() mutable {
        // call accept so we're back within the class scope (avoid lots of self)
        self->accept(std::forward<Basic>(packet));
      });
}

uint16_t Queued::length() {
  uint16_t len = 0;

  len += _packet_len[0] << 8;
  len += _packet_len[1];

  if (false) { // debug
    auto constexpr f = FMT_STRING("{} {} len={}\n");
    fmt::print(f, runTicks(), fnName(), len);
  }

  return len;
}

int64_t Queued::packetCount() const {
  int64_t count = 0;

  for (const Spool &spool : _spools) {
    count += spool.size();
  }

  return count;
}

void Queued::stats() {
  stats_timer.expires_after(10s);

  stats_timer.async_wait(  // wait on the timer
      asio::bind_executor( // will use an alternate executor
          local_strand,    // our local strand
          [self = shared_from_this()](error_code ec) {
            if (ec != errc::success) { // bail out on error (or cancel)
              return;
            }

            const auto &spools = self->_spools;

            static int64_t size_last = 0;
            const int64_t size_now = self->packetCount();

            const int64_t diff = size_now - size_last;

            constexpr auto f = FMT_STRING("{} {} spools={:02} "
                                          "rtp_count={:<5} diff={:>+6} "
                                          "seq_a/b={:>8}/{:<8}"
                                          "ts_a/b={:>12}/{:<12}"
                                          "network_time={} peaks={}/{}\n");

            uint32_t seq_a, seq_b = 0;
            uint64_t network_time, ts_a, ts_b;
            Freq peak_left, peak_right = -1;

            if (size_now > 0) { // spools could be empty
              auto &rtp_a = spools.front().front();
              auto &rtp_b = spools.back().back();

              seq_a = rtp_a->seq_num;
              seq_b = rtp_b->seq_num;

              ts_a = rtp_a->timestamp;
              ts_b = rtp_b->timestamp;

              const auto &anchor_data = Anchor::ptr()->getData();
              if (anchor_data.valid) {
                network_time = anchor_data.networkTime;
              }

              const auto &spool_last = spools.back();
              const auto &rtp_mid = spool_last[spool_last.size() / 2];

              if (rtp_mid->isReady()) {
                peak_left = rtp_mid->peaksLeft()->majorPeak().frequency();
                peak_right = rtp_mid->peaksRight()->majorPeak().frequency();
              }
            }

            fmt::print(f, runTicks(), moduleId,      // standard logging prefix
                       spools.size(),                // overall spool count
                       size_now, diff, seq_a, seq_b, // general stats and seq_nums
                       ts_a, ts_b,                   // timestamps
                       network_time,                 // network time
                       peak_left, peak_right);       // latest peaks count

            size_last = size_now; // save last size
            self->stats();        // schedule next stats report
          }));
}

void Queued::teardown() {
  asio::post(local_strand, [self = shared_from_this()]() mutable {
    [[maybe_unused]] error_code ec;
    self->stats_timer.cancel(ec);

    self->_spools.clear();
    self->_spools.shrink_to_fit();

    self->_packet.clear();
    self->_packet.shrink_to_fit();

    RTP::shkClear();
  });
}

} // namespace player
} // namespace pierre
