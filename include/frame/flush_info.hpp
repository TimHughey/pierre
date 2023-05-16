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

#pragma once

#include "base/asio.hpp"
#include "base/types.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/prepend.hpp>
#include <fmt/format.h>
#include <functional>
#include <future>
#include <optional>
#include <utility>

namespace pierre {

template <typename T>
concept HasSeqNumAndTimestamp = requires(T &f) {
  { f.sn() } noexcept;
  { f.ts() } noexcept;
};

template <typename T>
concept IsFramesContainer = requires(T &v) {
  { v.begin() } noexcept;
  { v.end() } noexcept;
  { v.empty() } noexcept;
};

template <typename T>
concept IsFrameStateVal = std::is_enum<T>::value;

using strand_ioc = asio::strand<asio::io_context::executor_type>;

struct FlushInfo {
  enum kind_t { All, Inactive };
  enum msg_t : uint8_t { BUSY_MSG = 0, EOM };

  FlushInfo(const seq_num_t from_seq, const timestamp_t from_ts, //
            const seq_num_t until_seq, const timestamp_t until_ts) noexcept
      : // enable this flush request
        a_active(std::make_unique<std::atomic_flag>()),
        // initialize flushed count to zero
        a_count(std::make_unique<std::atomic_int64_t>(0)),
        // set optional fields
        from_seq(from_seq), from_ts(from_ts),
        // flush everything <= seq_num
        until_seq(until_seq),
        // flush everything <= timestamp
        until_ts(until_ts) //
  {
    // atomic_flag can not be set via constructor, do so here
    a_active->test_and_set();

    // UNSURE IF THE FOLLOWING IS CURRENT OR LEGACY
    // a flush with from[seq|ts] will not be followed by a setanchor (i.e. render)
    // if it's a flush that will be followed by a set_anchor then stop render now.
  }

  FlushInfo(kind_t kind) noexcept {
    if (kind == All) {
      a_active = std::make_unique<std::atomic_flag>();
      a_active->test();
      all = true;
    }
  }

  FlushInfo() = default;
  FlushInfo(const FlushInfo &) = default;
  FlushInfo(FlushInfo &&) = default;

  bool active() const noexcept { return (bool)a_active && a_active->test(); }

  bool busy_hold(strand_ioc &exec) noexcept {
    std::promise<bool> prom;
    std::future<bool> fut(prom.get_future());

    asio::dispatch(exec, // use dispatch in case called from same executor
                   asio::prepend(
                       [this](std::promise<bool> prom) {
                         // we attempt this to be low-impact and immediately
                         // set the promise when not busy
                         if (!active()) {
                           prom.set_value(true);

                         } else {
                           // otherwise we need to handle any previous promises made
                           // before saving the new promise for setting by done
                           if (busy_prom.has_value()) {
                             msgs[BUSY_MSG] = "pending busy promise";

                             // set to false so the caller holding the previous
                             // busy future can detect it's been preempted
                             busy_prom->set_value(false);
                             busy_prom.reset();
                           } else {
                             msgs[BUSY_MSG].clear();
                           }

                           busy_prom.emplace(std::move(prom));
                         }
                       },
                       std::move(prom)));

    // block the caller until the promise is set
    return fut.get();
  }

  /// @brief Determine if the frame should be kept
  /// @tparam T Any object that has seq_num and timestamp data members
  /// @param frame What to examine
  /// @return boolean indicating if frame meets flush request criteria
  template <typename T, typename S>
    requires HasSeqNumAndTimestamp<T> && IsFrameStateVal<S>
  bool check(T &f, S state_v) noexcept {

    if (!active()) return true;

    auto flush{true};
    flush &= std::cmp_less_equal(f.sn(), until_seq);
    flush &= std::cmp_less_equal(f.ts(), until_ts);

    if (flush) f = state_v;

    return f != state_v;
  }

  template <typename T>
    requires HasSeqNumAndTimestamp<T>
  bool check(T &f) noexcept {

    if (!active()) return true;

    auto flush{true};
    flush &= std::cmp_less_equal(f.sn(), until_seq);
    flush &= std::cmp_less_equal(f.ts(), until_ts);

    if (flush) std::atomic_fetch_add(a_count.get(), 1);

    return flush;
  }

  auto count() noexcept { return std::atomic_exchange(a_count.get(), 0); }

  void done() noexcept {
    from_seq = 0;
    from_ts = 0;
    until_seq = 0;
    until_ts = 0;

    a_active->clear();
    a_active->notify_all();

    if (busy_prom.has_value()) {
      busy_prom->set_value(true);
      busy_prom.reset();
    }
  }

  /// @brief Reset count of frames flushed
  /// @return number of frames frames
  int64_t flushed() noexcept { return std::atomic_exchange(a_count.get(), 0); }

  /// @brief Active status of flush
  /// @return true if active, otherwise false
  explicit operator bool() const noexcept { return active(); }

  FlushInfo &operator=(FlushInfo &&) = default;

  /// @brief Flush the frame if it meets the flush criteria
  /// @tparam T An object that contains seq_num and timestamp data members
  /// @param frame
  /// @return boolean indicating if frame was flushed
  template <typename T, typename S>
    requires HasSeqNumAndTimestamp<T> && IsFrameStateVal<S>
  auto operator()(T &f, S state_v) noexcept {

    if ((f.sn() <= until_seq) && (f.ts() <= until_ts)) {
      // flush this frame
      f = state_v;
      std::atomic_fetch_add(a_count.get(), 1);
    }

    return f == state_v;
  }

  template <typename T, typename S>
    requires IsFramesContainer<T> && IsFrameStateVal<S>
  auto operator()(T &frames, S state_v) noexcept {

    if (frames.size()) {
      auto begin = frames.begin();
      auto end = frames.end();

      // lambda for determining if a frame should be flushed
      auto check_frame = [=, this](auto &f) { return !check(f, state_v); };
      auto to_it = std::find_if(begin, end, check_frame);

      if (to_it != end) {
        std::atomic_fetch_add(a_count.get(), std::distance(begin, to_it));
      }
    }

    return flushed();
  }

  const string pop_msg(msg_t mt) noexcept {
    auto msg = msgs[mt];
    msgs[mt].clear();

    return msg;
  }

public:
  // order dependent
  std::unique_ptr<std::atomic_flag> a_active{nullptr};
  std::unique_ptr<std::atomic_int64_t> a_count{nullptr};
  seq_num_t from_seq{0};
  timestamp_t from_ts{0};
  seq_num_t until_seq{0};
  timestamp_t until_ts{0};
  bool all{false};

  // order independent
  std::optional<std::promise<bool>> busy_prom;
  std::array<string, EOM> msgs;
};

} // namespace pierre

/// @brief Custom formatter for FlushInfo
template <> struct fmt::formatter<pierre::FlushInfo> : fmt::formatter<std::string> {

  // parse is inherited from formatter<string>.
  template <typename FormatContext>
  auto format(const pierre::FlushInfo &fi, FormatContext &ctx) const {
    auto msg = fmt::format("FLUSH_INFO {}", fi.active() ? "ACTIVE " : "INACTIVE ");
    auto w = std::back_inserter(msg);

    if (fi.all) fmt::format_to(w, "ALL ");
    if (fi.busy_prom.has_value()) fmt::format_to(w, "WAITER ");
    if (fi.from_seq) fmt::format_to(w, "*FROM sn={:<8} ts={:<12} ", fi.from_seq, fi.from_ts);
    if (fi.until_seq) fmt::format_to(w, "UNTIL sn={:<8} ts={:<12} ", fi.until_seq, fi.until_ts);

    return formatter<std::string>::format(msg, ctx);
  }
};
