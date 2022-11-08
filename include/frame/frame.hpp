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

#include "base/anchor_data.hpp"
#include "base/anchor_last.hpp"
#include "base/elapsed.hpp"
#include "base/input_info.hpp"
#include "base/pet.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "frame/peaks.hpp"
#include "frame/state.hpp"

#include <array>
#include <compare>
#include <exception>
#include <future>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <tuple>

namespace pierre {

using cipher_buff_t = std::array<uint8_t, 16 * 1024>;
using cipher_buff_ptr = std::unique_ptr<cipher_buff_t>;

class Frame;
using frame_t = std::shared_ptr<Frame>;
using frame_future = std::shared_future<frame_t>;
using frame_promise = std::promise<frame_t>;

class Frame : public std::enable_shared_from_this<Frame> {
private:
  Frame(uint8v &packet) noexcept;

public:
  static frame_t create(uint8v &packet) noexcept { return frame_t(new Frame(packet)); }

  // Public API
  bool decipher(uint8v &packet) noexcept;
  bool deciphered() const noexcept { return state >= frame::state(frame::DECIPHERED); }
  bool decode();
  void flushed() noexcept { state = frame::FLUSHED; }

  static void init(); // Digital Signal Analysis (hidden in .cpp)

  void mark_rendered() { state = frame::RENDERED; }

  bool silent() const noexcept {
    auto [left, right] = peaks;

    return Peaks::silence(left) && Peaks::silence(right);
  }

  //
  // state_now();
  //
  frame::state state_now(AnchorLast anchor, const Nanos &lead_time = InputInfo::lead_time);

  Nanos sync_wait() const noexcept { return _sync_wait.value_or(InputInfo::lead_time_min); }
  static bool sync_wait_ok(frame_t f) noexcept { return f && f->sync_wait_ok_safe(); }
  bool sync_wait_ok_safe() const noexcept { return _sync_wait.has_value(); }
  Nanos sync_wait_recalc(); // can throw if no anchor

  // misc debug
  const string inspect(bool full = false);
  static const string inspect_safe(frame_t frame, bool full = false);
  void log_decipher() const;

private:
  Nanos set_sync_wait(const Nanos diff) noexcept {
    if ((_sync_wait.has_value() == false) || (_sync_wait.value_or(0ms) != diff)) {
      _sync_wait.emplace(diff);
    }

    return _sync_wait.value();
  }

public:
  // order dependent
  const Nanos created_at;
  const Nanos lead_time;
  frame::state state;

  uint8_t version{0x00};
  bool padding{false};
  bool extension{false};
  uint8_t ssrc_count{0};
  seq_num_t seq_num{0};
  timestamp_t timestamp{0};
  uint32_t ssrc{0};
  cipher_buff_ptr m;

  // decipher support
  int cipher_rc{0};
  unsigned long long decipher_len{0}; // type dictated by libsodium

  // decode frame
  int samples_per_channel = 0;
  int channels = 0;

  // populated by DSP
  std::tuple<peaks_t, peaks_t> peaks;

  // calculated by state_now() or recalculated by sync_wait_recalc()
  std::optional<Nanos> _sync_wait;

private:
  // order independent
  std::optional<AnchorLast> _anchor;

  // a short frame is sometimes sent for an unknown purpose
  // detect those frames and don't send for further processing
  static constexpr size_t SHORT_LEN{0};

public:
  static constexpr csv module_id{"FRAME"};
};

} // namespace pierre