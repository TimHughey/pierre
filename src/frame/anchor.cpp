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

#include "anchor.hpp"
#include "base/anchor_data.hpp"
#include "base/input_info.hpp"
#include "base/io.hpp"
#include "base/pet.hpp"
#include "base/typical.hpp"
#include "master_clock.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fmt/format.h>
#include <memory>
#include <optional>
#include <ranges>
#include <vector>

namespace pierre {

namespace shared {
std::optional<Anchor> anchor;
} // namespace shared

// destructor, singleton static functions
void Anchor::init() { shared::anchor.emplace(); }

/* int get_ptp_anchor_local_time_info(rtsp_conn_info *conn, uint32_t *anchorRTP,
                                   uint64_t *anchorLocalTime) {
  uint64_t actual_clock_id, actual_time_of_sample, actual_offset, start_of_mastership;
  int response = ptp_get_clock_info(&actual_clock_id, &actual_time_of_sample, &actual_offset,
                                    &start_of_mastership);

  if (response == clock_ok) {


    if (conn->anchor_remote_info_is_valid !=
        0) { // i.e. if we have anchor clock ID and anchor time / rtptime
      // figure out how long the clock has been master
      int64_t duration_of_mastership = time_now - start_of_mastership;
      // if we have an alternative, i.e. if last anchor stuff is valid
      // then we can wait for a long time to let the new master settle
      // if we do not, we can wait for some different (shorter) time before
      // using the master clock timing

      if (actual_clock_id == conn->anchor_clock) {
        // if the master clock and the anchor clock are the same
        // wait at least this time before using the new master clock
        // note that mastership may be backdated
        if (duration_of_mastership < 1500000000) {
          debug(3, "master not old enough yet: %f ms", 0.000001 * duration_of_mastership);
          response = clock_not_ready;
        } else if ((duration_of_mastership > 5000000000) ||
                   (conn->last_anchor_info_is_valid == 0)) {
          // use the master clock if it's at least this old or if we have no alternative
          // and it at least is the minimum age.
          conn->last_anchor_rtptime = conn->anchor_rtptime;
          conn->last_anchor_local_time = conn->anchor_time - actual_offset;
          conn->last_anchor_time_of_update = time_now;
          if (conn->last_anchor_info_is_valid == 0)
            conn->last_anchor_validity_start_time = start_of_mastership;
          conn->last_anchor_info_is_valid = 1;
          if (conn->anchor_clock_is_new != 0)
            debug(1,
                  "Connection %d: Clock %" PRIx64
                  " is now the new anchor clock and master clock. History: %f milliseconds.",
                  conn->connection_number, conn->anchor_clock, 0.000001 * duration_of_mastership);
          conn->anchor_clock_is_new = 0;
        }
      } else {
        // the anchor clock and the actual clock are different
        // this could happen because
        // the master clock has changed or
        // because the anchor clock has changed

        // so, if the anchor has not changed, it must be that the master clock has changed
        if (conn->anchor_clock_is_new != 0)
          debug(3,
                "Connection %d: Anchor clock has changed to %" PRIx64 ", master clock is: %" PRIx64
                ". History: %f milliseconds.",
                conn->connection_number, conn->anchor_clock, actual_clock_id,
                0.000001 * duration_of_mastership);

        if ((conn->last_anchor_info_is_valid != 0) && (conn->anchor_clock_is_new == 0)) {
          int64_t time_since_last_update =
              get_absolute_time_in_ns() - conn->last_anchor_time_of_update;
          if (time_since_last_update > 5000000000) {
            debug(1,
                  "Connection %d: Master clock has changed to %" PRIx64
                  ". History: %f milliseconds.",
                  conn->connection_number, actual_clock_id, 0.000001 * duration_of_mastership);
            // here we adjust the time of the anchor rtptime
            // we know its local time, so we use the new clocks's offset to
            // calculate what time that must be on the new clock

            // Now, the thing is that while the anchor clock and master clock for a
            // buffered session starts off the same,
            // the master clock can change without the anchor clock changing.
            // SPS allows the new master clock time to settle down and then
            // calculates the appropriate offset to it by
            // calculating back from the local anchor information and the new clock's
            // advertised offset. Of course, small errors will occur.
            // More importantly, the new master clock(s) and the original one will
            // drift at different rates. So, after all this, if the original master
            // clock becomes the master again, then there could be quite a difference
            // in the time information that was calculated through all the clock
            // changes and the actual master clock's time information.
            // What do we do? We can hardly ignore this new and reliable information
            // so we'll take it. Maybe we should add code to slowly correct towards it
            // but at present, we just take it.

            // So, if the master clock has again become equal to the actual anchor clock
            // then we can reinstate it all.
            // first, let us calculate the cumulative offset after swapping all the clocks...
            conn->anchor_time = conn->last_anchor_local_time + actual_offset;

            // we can check how much of a deviation there was going from clock to clock and back
            // around to the master clock

            if (actual_clock_id == conn->actual_anchor_clock) {
              int64_t cumulative_deviation = conn->anchor_time - conn->actual_anchor_time;
              debug(1,
                    "Master clock has become equal to the anchor clock. The estimated clock time "
                    "was %f ms ahead(+) or behind (-) the real clock time.",
                    0.000001 * cumulative_deviation);
              conn->anchor_clock = conn->actual_anchor_clock;
              conn->anchor_time = conn->actual_anchor_time;
              conn->anchor_rtptime = conn->actual_anchor_rtptime;
            } else {
              // conn->anchor_time = conn->last_anchor_local_time + actual_offset; // already done
              conn->anchor_clock = actual_clock_id;
            }
            conn->anchor_clock_is_new = 0;
          }
        } else {
          response = clock_not_valid; // no current clock information and no previous clock info
        }
      }
    } else {
      // debug(1, "anchor_remote_info_is_valid not valid");
      response = clock_no_anchor_info; // no anchor information
    }
  }
} */

// general API and member functions
AnchorLast Anchor::get_data(const ClockInfo clock) {

  // do nothing if we've not received anchor data from the source
  if (remote_valid == false) {
    return AnchorLast();
  }

  auto &live = datum(Datum::live);

  if (live.match_clock_id(clock)) {
    live.set_master_for(clock);

    if (clock.is_stable() || (_last.has_value() == false)) {
      // only establish last if clock is of minimum age
      if (clock.is_minimum_age()) {
        _last.emplace(live.clock_id, live.rtp_time, live.anchor_time, clock);

        live.log_new_master_if_needed(data_new);
      }
    }
  } else {
    static Elapsed last_report;
    static uint64_t not_handled = 0;
    ++not_handled;

    if ((not_handled == 1) || (last_report() > 500ms)) {
      __LOG0(LCOL01 " not handled, clocks -> live={:#016x} master={:#016x}\n", //
             module_id, "WARN", live.clock_id, clock.clock_id);
      last_report.reset();
    }
  }

  data_new = false; // new data was processed above

  return _last.value_or(AnchorLast());
}

void Anchor::handle_new_data(const AnchorData &ad) {
  const auto &live = cdatum(Datum::live);

  if (ad.details_changed(live)) {
    data_new = true;

    if (log_data_new) {
      string msg;
      auto w = std::back_inserter(msg);
      std::vector<string> lines;

      auto clock_future = shared::master_clock->info(Nanos::zero());
      const auto clock = clock_future.get();

      lines.emplace_back(fmt::format("{:<7}: clock={:#018x} rtp_time={} anchor={}", "old",
                                     live.clock_id, live.rtp_time,
                                     pet::humanize(live.anchor_time)));

      lines.emplace_back(fmt::format("{:<7}: clock={:#018x} rtp_time={} anchor={}", "new",
                                     ad.clock_id, ad.rtp_time, pet::humanize(ad.anchor_time)));

      lines.emplace_back(fmt::format("{:<7}: clock={:#018x} sample_time={}", "master",
                                     clock.clock_id, pet::humanize(clock.sample_time())));

      ranges::for_each(lines, [&w](const auto &line) { //
        fmt::format_to(w, "{}{}\n", LOG_INDENT, line);
      });

      __LOG0(LCOL01 " parameters have changed data_new={}\n{}", module_id, "INFO", data_new, msg);
    }
  }
}

void Anchor::handle_quick_change(const AnchorData &ad) {
  const auto &live = cdatum(Datum::live);

  if (live.match_clock_id(ad) && !live.match_details(ad) && _last.has_value()) {

    if (const auto valid_for = _last->valid; valid_for < ClockInfo::AGE_STABLE) {
      __LOG0(LCOL01 " parameters changed before clock={:#016x} stabilized, valid_for={}\n",
             module_id, "INFO", _last->clock_id, valid_for);

      _last.reset(); // last is no longer valid
    }
  }
}

/*

const AnchorData &Anchor::get_data() {
  auto clock = shared::master_clock->info();
  auto &live = datum(Datum::live);
  auto &source = datum(Datum::source);

  if (clock.ok()) {

    auto &last = datum(Datum::last);

    if (live.valid()) { // live is valid
      // the master clock and the source clock are the same
      if (live == clock) {
        if (live.is_new) {
          __LOG0(LCOL01 " clock_id match, is_new={}\n", module_id, "GET_DATA", live.is_new);
        }

        if (clock.not_minimum_age()) {
          __LOG0(LCOL01 " master clock too young age={}\n", module_id, "GET_DATA",
                 clock.master_for());
          // do nothing, handle which anchor to return below
        } else if (clock.is_stable() || live.not_valid()) {

          // master clock can be used if master for at least 1.5s when no other option
          // is available

          last.rtp_time = live.rtp_time;
          last.anchor_time = live.anchor_time;
          last.localized = pet::subtract_offset(live.anchor_time, clock.rawOffset);

          if (last.valid_at == Nanos::zero()) {
            last.valid_at = clock.mastershipStartTime;

            last.localized_elapsed.reset();
          }

          if (live.is_new) {
            __LOG(LCOL01 " clock={:#x} is now master and anchor_new={}\n", //
                  module_id, "NEW_CLOCK", live.clock_id, live.is_new);
            live.is_new = false;
          }
        }
      } else {
        if (live.is_new) {
          __LOG(LCOL01 " clock={:#x} is now master _new={}\n", //
                module_id, "NEW_CLOCK", live.clock_id, live.is_new);
        }

        // NOTE:
        //  1. live will not be new if master clock has changed
        //  2. Anchor::save() decides if live is new
        if (live.valid_for_at_least(ClockInfo::AGE_STABLE) && live.not_new()) {
          live.anchor_time = pet::datd_offset(last.anchor_time, clock.rawOffset);

          // we can check how much of a deviation there was going from clock to clock and back
          // around to the master clock

          if (source == clock) {
            auto cumulative_deviation = live.anchor_time - source.anchor_time;
            __LOG0(LCOL01
                   " master clock has become equal to the anchor clock. The estimated clock time "
                   "was {} ahedat(+) or behind (-) the real clock time.",
                   module_id, "SOURCE_CLOCK", pet::humanize(cumulative_deviation));
            live.anchor_time = source.anchor_time;
            live.clock_id = source.clock_id;
            live.rtp_time = source.rtp_time;
          } else {
            // conn->anchor_time = conn->last_anchor_local_time + actual_offset; // alredaty done
            live.change_clock_id(clock);
          }
          live.is_new = false;

          // only allow live anchor change after 5s

          if (source == clock) { // source clock has become master
            live = source;

          } else { // this is a new master anchor clock, just update live clock_id
            live.change_clock_id(clock);
          }
        }
      } // end of master compare to source

    } // end live valid
  }   // end clock ok

  return datum(Datum::last).valid() ? datum(Datum::last) : datum(Datum::invalid);
}

*/

void Anchor::save_impl(AnchorData ad) {
  handle_new_data(ad);

  // empty clock info, reset Anchor and return
  if (ad.empty()) {
    teardown();
    return;
  }

  handle_quick_change(ad);

  remote_valid = true;

  datum(Datum::live) = ad;
  datum(Datum::source) = ad;
}

void Anchor::teardown() {
  remote_valid = false;
  data_new = false;
  _datum.fill(AnchorData());
}

// logging and debug

} // namespace pierre