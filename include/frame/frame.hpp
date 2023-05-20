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

#include "base/input_info.hpp"
#include "base/pet_types.hpp"
#include "base/random.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "frame/clock_info.hpp"
#include "frame/fdecls.hpp"
#include "frame/peaks.hpp"
#include "frame/state.hpp"

#include <algorithm>
#include <cassert>
#include <compare>
#include <concepts>
#include <fmt/chrono.h>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>

namespace pierre {

using ull_t = long long unsigned int;

class Av;

template <typename F>
concept FrameProcessingState = requires(F &f) {
  { f.state } -> std::convertible_to<frame::state_now_t>;
  { f.sync_wait } -> std::convertible_to<Nanos>;
};

class Frame {
  friend struct fmt::formatter<Frame>;
  friend class Av;

public:
  /// @brief Construct a silent frame
  Frame() noexcept : state(frame::NONE), silence{true} {}
  Frame(const frame_state_v fsv) noexcept : state(fsv), silence{true} {

    switch ((frame_state_v)state) {
    case frame::SILENCE:
      state = frame::READY;
      cache_sync_wait(InputInfo::lead_time);
      break;
    case frame::SENTINEL:
      state = frame::SENTINEL;
      cache_sync_wait(InputInfo::lead_time);
      break;

    default:
      assert("invalid state");
    }
  }

  Frame(uint8v &packet) noexcept
      : state(frame::HEADER_PARSED),              // frame header parsed
        version((packet[0] & 0b11000000) >> 6),   // RTPv2 == 0x02
        padding((packet[0] & 0b00100000) >> 5),   // has padding
        extension((packet[0] & 0b00010000) >> 4), // has extension
        ssrc_count((packet[0] & 0b00001111)),     // source system record count
        ssrc(packet.to_uint32(8, 4)),             // source system record count
        seq_num(packet.to_uint32(1, 3)),          // rtp seq num note: only three bytes
        timestamp(packet.to_uint32(4, 4))         // rtp timestamp
  {}

public:
  ~Frame() noexcept;
  Frame(Frame &&) = default;

  Frame &operator=(Frame &&) = default;
  void operator=(frame_state_v fsv) noexcept { state = fsv; }

  explicit operator frame::state_now_t() const noexcept { return (frame::state_now_t)state; }
  std::weak_ordering operator<=>(const Frame &rhs) const noexcept { return ts() <=> rhs.ts(); }
  bool operator<(const frame_state_v lhs) const noexcept { return state < lhs; }
  bool operator>(const frame_state_v lhs) const noexcept { return state > lhs; }
  bool operator==(const frame_state_v lhs) const noexcept { return state == lhs; }
  bool operator!=(const frame_state_v lhs) const noexcept { return !(*this == lhs); }

  static void init() noexcept;

  ////
  //// Frame state checks
  ////
  bool can_render() const noexcept { return state.can_render(); }
  bool dont_render() const noexcept { return !can_render(); }

  /// @brief Mark frame as flushed and record it's state
  void flush() noexcept;
  bool flushed() const noexcept { return state.flushed(); }
  bool future() noexcept { return state.future(); }

  const Peaks &get_peaks() noexcept { return peaks; }

  bool live() const noexcept { return !synthetic(); }

  auto mark_rendered() noexcept {
    state = silent() ? frame::SILENCE : frame::RENDERED;
    return state.record_state();
  }

  auto no_timing() const noexcept { return state == frame::NO_CLK_ANC; }
  auto outdated() const noexcept { return state == frame::OUTDATED; }

  bool ready() const noexcept { return state == frame::READY; }
  bool ready_or_future() const noexcept { return state.ready_or_future(); }
  bool sentinel() const noexcept { return state.sentinel(); }

  bool silent() const noexcept { return silence; }
  bool silent(bool is_silent) noexcept { return std::exchange(silence, is_silent); }

  /// @brief Frames not assigned a sender seq_num or timestamp are
  ///        considered 'synthetic' and do not contain Peaks
  /// @return boolean
  bool synthetic() const noexcept { return !seq_num || !timestamp; }

  /// @brief Decipher, decode and find peaks (dsp)
  /// @param packet
  /// @param key
  /// @return
  auto process(uint8v packet, const uint8v &key) noexcept {
    return (state.header_ok()) //
               ? decipher(std::move(packet), key)
               : (frame_state_v)state;
  }

  frame::state record_state() const noexcept { return state.record_state(); }

  void record_sync_wait() const noexcept;

  struct sample_info {
    int channels{0};
    int samp_per_ch{0};
  };

  void save_samples_info(const sample_info info) noexcept {
    channels = info.channels;
    samp_per_ch = info.samp_per_ch;
  }

  /// @brief Sequence Number assigned by sender
  /// @return unsigned int
  constexpr seq_num_t sn() const noexcept { return seq_num; }

  /// @brief Get the frame's current state
  /// @return frame_state_v
  frame_state_v state_now() const noexcept { return (frame_state_v)state; }

  /// @brief Calculate the state of the frame as of the current system time.
  ///        The frame's state is set to NO_CLOCK or NO_ANCHOR in the
  ///        event neither MasterClock or Anchor are ready.
  ///
  ///        NOTE: The state of synthetic frames are never calculated.
  /// @param clk reference to the MasterClock
  /// @param anc reference to Anchor as maintained by the sender
  /// @return calculated frame::state
  // frame::state state_now(MasterClock &clk, Anchor &anc) noexcept;

  frame::state state_now(AnchorLast &ancl) noexcept;

  /// @brief Calculated duration to wait to syncronize frame to sender's view
  ///        of 'playback' (aka render) timeline.
  ///
  ///        NOTE: value returned is the most current of:
  ///         -at the time the frame's state was determined
  ///         -the most recent call to sync wait specifying MasterClock and Anchor
  ///         -value at construction (0ns)
  /// @return nanoseconds

  Nanos sync_wait() const noexcept { return cached_sync_wait.value_or(InputInfo::lead_time); }

  /// @brief Calculate (or recalculate) the sync wait duration and cache the value
  ///
  ///        This function is used in two scenarios:
  ///          -determination of the frame's state
  ///          -to provide the most accurate sync wait since calculating the frame's state
  ///
  ///        Said differently, calling this function to calculate the most accurate sync wait
  ///        before handling the next frame improves syncronization to the sender's timeline.
  ///
  ///        NOTE: The behavior is the same as calling sync_wait() if neither the
  ///        MasterClock or Anchor are ready.
  /// @param clk reference to the MasterClock
  /// @param anc reference to Anchor as set by sender and used to convert sender's
  ///            timeline to local system time
  /// @return nanoseconds
  Nanos sync_wait(MasterClock &clk, Anchor &anc) noexcept {
    if (synthetic()) return sync_wait();

    sync_wait(clk, anc, ts(), *cached_sync_wait);

    return sync_wait();
  }

  static bool sync_wait(MasterClock &clk, Anchor &anc, ftime_t t, Nanos &sync_wait) noexcept;

  template <typename FRR>
    requires FrameProcessingState<FRR>
  static void sync_wait(MasterClock &clk, Anchor &anc, FRR &frr) noexcept {
    sync_wait(clk, anc, frr.ts, frr.sync_wait);
  }

  /// @brief Timestamp assigned by sendor
  /// @param scale (optional) divide timestamp by this value
  /// @return unisgned int
  ftime_t ts(uint32_t scale = 0) const noexcept { return scale ? timestamp / scale : timestamp; }

private:
  /// @brief Explictly set the sync wait to a duration
  ///
  ///        NOTE: Synthetic frame sync wait will not be set by this function
  /// @param diff nanoseconds
  /// @return nanoseconds
  const Nanos cache_sync_wait(Nanos diff) noexcept { return cached_sync_wait.emplace(diff); }

  frame_state_v decipher(uint8v packet, const uint8v key) noexcept;

  void log_decipher(int cipher_rc, ull_t consumed, const uint8v &p, const uint8v &m) const noexcept;

private:
  // order dependent
  frame::state state;
  uint8_t version{0x00};
  bool padding{false};
  bool extension{false};
  uint8_t ssrc_count{0};
  uint32_t ssrc{0};
  seq_num_t seq_num{0};
  ftime_t timestamp{0};

  // order independent
  static std::unique_ptr<Av> av;
  int samp_per_ch{0};
  int channels{0};

  random rand_gen;
  std::optional<Nanos> cached_sync_wait;
  bool silence{false};

  // populated by Av or empty (silent)
  Peaks peaks;

public:
  MOD_ID("frame");
};

} // namespace pierre

/// @brief Custom formatter for Frame
template <> struct fmt::formatter<pierre::Frame> : formatter<std::string> {
  using millis_fp = std::chrono::duration<double, std::chrono::milliseconds::period>;
  using frame = pierre::Frame;

  // parse is inherited from formatter<string>.
  template <typename FormatContext> auto format(const frame &f, FormatContext &ctx) const {
    std::string msg;
    auto w = std::back_inserter(msg);

    const auto sync_wait = std::chrono::duration_cast<millis_fp>(f.sync_wait());

    if (f.synthetic()) fmt::format_to(w, "SYNTHETIC ");

    fmt::format_to(w, "{} sw={:0.4}", f.state, sync_wait);

    if (!f.synthetic()) fmt::format_to(w, " sn={:x} ts={:x}", f.sn(), f.ts());

    return formatter<std::string>::format(msg, ctx);
  }
};