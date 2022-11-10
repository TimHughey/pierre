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
  Frame();
  Frame(uint8v &packet) noexcept;

public:
  static frame_t create(uint8v &packet) noexcept { return frame_t(new Frame(packet)); }

  // Public API
  bool decipher(uint8v &packet) noexcept;
  bool deciphered() const noexcept { return state >= frame::state(frame::DECIPHERED); }
  bool decode() noexcept;
  void flushed() noexcept { state = frame::FLUSHED; }

  static void init(); // Digital Signal Analysis (hidden in .cpp)

  void mark_rendered() noexcept { state = frame::RENDERED; }

  bool silent() const noexcept { return peaks.silence(); }

  // state_now(), sync_wait() and related functions can be overriden by subclasses
  virtual frame::state state_now(AnchorLast anchor = AnchorLast(),
                                 const Nanos &lead_time = InputInfo::lead_time) noexcept;

  virtual Nanos sync_wait() const noexcept { return _sync_wait.value_or(InputInfo::lead_time_min); }
  static bool sync_wait_ok(frame_t f) noexcept { return f && f->sync_wait_ok_safe(); }
  bool sync_wait_ok_safe() const noexcept { return _sync_wait.has_value(); }

  // can throw if no anchor, in .cpp to limit exception include
  virtual Nanos sync_wait_recalc() noexcept;

  // misc debug
  const string inspect(bool full = false) const noexcept;
  static const string inspect_safe(frame_t frame, bool full = false) noexcept;
  void log_decipher() const noexcept;

protected:
  Nanos set_sync_wait(const Nanos diff) noexcept {
    if ((_sync_wait.has_value() == false) || (_sync_wait.value() != diff)) {
      _sync_wait.emplace(diff);
    }

    return _sync_wait.value();
  }

public:
  // order dependent
  const Nanos created_at;
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
  cipher_buff_ptr m;                  // temporary buffer of ciphered data
  unsigned long long decipher_len{0}; // type dictated by libsodium

  // decode frame (exposed publicly for av decode)
  int samples_per_channel{0};
  int channels{0};

  // populated by DSP or empty (silent)
  Peaks peaks;

protected:
  // calculated by state_now() or recalculated by sync_wait_recalc()
  std::optional<Nanos> _sync_wait;

private:
  // order independent
  int cipher_rc{0}; // decipher support

  std::optional<AnchorLast> _anchor;

public:
  static constexpr uint8_t RTPv2{0x02};
  static constexpr csv module_id{"FRAME"};
};

} // namespace pierre