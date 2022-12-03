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
#include "base/pet.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "frame/anchor_data.hpp"
#include "frame/anchor_last.hpp"
#include "frame/peaks.hpp"
#include "frame/state.hpp"

#include <array>
#include <future>
#include <memory>
#include <optional>

namespace pierre {

using cipher_buff_t = std::optional<uint8v>;

class Frame;
using frame_t = std::shared_ptr<Frame>;
using frame_future = std::shared_future<frame_t>;
using frame_promise = std::promise<frame_t>;

class Frame : public std::enable_shared_from_this<Frame> {

private: // use create()
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

protected: // subclass use only
  // creates a Frame with an initial state and sync_wait
  Frame(const frame::state_now_t s, const Nanos swait = InputInfo::lead_time) noexcept
      : state(s),         // flag frame as empty
        _sync_wait(swait) // defaults to lead time
  {}

public:
  static auto create(uint8v &packet) noexcept { //
    return std::shared_ptr<Frame>(new Frame(packet));
  }

  auto ptr() noexcept { return shared_from_this(); }

  // Public API
  bool decipher(uint8v packet, const uint8v key) noexcept;
  bool deciphered() const noexcept { return state >= frame::state(frame::DECIPHERED); }
  bool decode() noexcept;
  void flushed() noexcept { state = frame::FLUSHED; }

  static void init(); // Digital Signal Analysis (hidden in .cpp)

  void mark_rendered() noexcept { state = frame::RENDERED; }

  bool silent() const noexcept { return peaks.silence(); }

  frame::state state_now(AnchorLast anchor, const Nanos &lead_time = InputInfo::lead_time) noexcept;
  frame::state state_now(const Nanos diff, const Nanos &lead_time = InputInfo::lead_time) noexcept;

  // sync_wait() and related functions can be overriden by subclasses
  virtual Nanos sync_wait() const noexcept { return _sync_wait.value_or(InputInfo::lead_time_min); }
  static bool sync_wait_ok(frame_t f) noexcept { return f && f->sync_wait_ok_safe(); }
  bool sync_wait_ok_safe() const noexcept { return _sync_wait.has_value(); }

  // returns sync_wait unchanged if anchor is not available
  virtual Nanos sync_wait_recalc() noexcept;

  // misc debug
  const string inspect(bool full = false) const noexcept;
  static const string inspect_safe(frame_t frame, bool full = false) noexcept;
  void log_decipher() const noexcept;

protected:
  Nanos set_sync_wait(const Nanos diff) noexcept { return _sync_wait.emplace(diff); }

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

  // populated by DSP or empty (silent)
  Peaks peaks;

  // populated by Reel
  reel_serial_num_t reel{0};

protected:
  // calculated by state_now() or recalculated by sync_wait_recalc()
  std::optional<Nanos> _sync_wait;

private:
  // order independent
  int cipher_rc{0};                  // decipher support
  static ptrdiff_t cipher_buff_size; // in bytes, populated by init()

  std::optional<AnchorLast> _anchor;

public:
  static constexpr uint8_t RTPv2{0x02};
  static constexpr csv module_id{"FRAME"};
};

} // namespace pierre