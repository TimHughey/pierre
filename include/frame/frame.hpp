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

#include "base/elapsed.hpp"
#include "base/input_info.hpp"
#include "base/pet_types.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "frame/clock_info.hpp"
#include "frame/fdecls.hpp"
#include "frame/peaks.hpp"
#include "frame/state.hpp"

#include <algorithm>
#include <compare>
#include <fmt/chrono.h>
#include <memory>
#include <optional>

namespace pierre {

using cipher_buff_t = std::optional<uint8v>;

class Av;

class Frame {
  friend struct fmt::formatter<Frame>;

public:
  /// @brief Construct a silent frame
  Frame() noexcept : state(frame::READY) { silent(true); }

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

  std::weak_ordering operator<=>(const Frame &rhs) const noexcept {
    return timestamp <=> rhs.timestamp;
  }

  bool decipher(uint8v packet, const uint8v key) noexcept;
  bool deciphered() const noexcept { return state >= frame::state(frame::DECIPHERED); }
  bool decode() noexcept;
  void flushed() noexcept { state = frame::FLUSHED; }

  const Peaks &get_peaks() noexcept { return peaks; }

  void mark_rendered() noexcept {
    state = silent() ? frame::SILENCE : frame::RENDERED;

    state.record_state();
  }

  bool ready() const noexcept { return state.ready(); }

  frame::state record_state() const noexcept {
    state.record_state();

    return state;
  }

  bool silent() const noexcept { return silence; }

  bool silent(bool is_silent) noexcept {
    silence = is_silent;
    return silence;
  }

  frame::state state_now(MasterClock &clk, Anchor &anc) noexcept {
    state_now(clk, anc, InputInfo::lead_time);

    return state;
  }

  frame::state state_now(MasterClock &clk, Anchor &anc, Nanos lead_time) noexcept;

  // frame::state state_now(const AnchorLast anchor) noexcept;
  // frame::state state_now(const Nanos diff, const Nanos &lead_time = InputInfo::lead_time)
  // noexcept;

  // sync_wait() and related functions can be overriden by subclasses
  Nanos sync_wait() const noexcept { return cached_sync_wait.value_or(InputInfo::lead_time); }
  Nanos sync_wait(MasterClock &clk, Anchor &anc) noexcept;
  const Nanos sync_wait(Nanos diff) noexcept {
    return synthetic() ? sync_wait() : cached_sync_wait.emplace(diff);
  }

  bool synthetic() const noexcept { return !seq_num || !timestamp; }

  // misc debug
  void log_decipher() const noexcept;

public:
  // order dependent
  Elapsed lifespan;
  frame::state state;

private:
  // order dependent (no need to expose publicly)
  uint8_t version{0x00};
  bool padding{false};
  bool extension{false};
  uint8_t ssrc_count{0};
  uint32_t ssrc{0};

public:
  // order dependent (must expose publicly)
  seq_num_t seq_num{0};
  timestamp_t timestamp{0};

  // order independent
  cipher_buff_t m; // temporary buffer for ciphering

  // decode frame (exposed publicly for av decode)
  int samples_per_channel{0};
  int channels{0};

  // populated by Av or empty (silent)
  Peaks peaks;

  // calculated by state_now() or recalculated by sync_wait_recalc()
  std::optional<Nanos> cached_sync_wait;
  bool silence{false};

private:
  // order independent
  static std::unique_ptr<Av> av;
  int cipher_rc{0};                  // decipher support
  static ptrdiff_t cipher_buff_size; // in bytes, populated by init()

  ClockInfo clk_info;

public:
  static constexpr uint8_t RTPv2{0x02};
  static constexpr auto module_id{"frame"sv};
};

} // namespace pierre

/// @brief Custom formatter for Frame
template <> struct fmt::formatter<pierre::Frame> : formatter<std::string> {
  using millis_fp = std::chrono::duration<double, std::chrono::milliseconds::period>;

  // parse is inherited from formatter<string>.
  template <typename FormatContext> auto format(const pierre::Frame &f, FormatContext &ctx) const {
    std::string msg;
    auto w = std::back_inserter(msg);

    const auto sync_wait = std::chrono::duration_cast<millis_fp>(f.sync_wait());

    if (f.synthetic()) fmt::format_to(w, "synthetic ");

    fmt::format_to(w, "{} sn={} ts={} sw={:0.4}", f.state, f.seq_num, f.timestamp, sync_wait);

    return formatter<std::string>::format(msg, ctx);
  }
};