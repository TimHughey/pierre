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
#include "base/flush_request.hpp"
#include "base/input_info.hpp"
#include "base/pet.hpp"
#include "base/typical.hpp"
#include "base/uint8v.hpp"
#include "frame/peaks.hpp"
#include "frame/state.hpp"
#include "frame/types.hpp"

#include <compare>
#include <future>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <tuple>

namespace pierre {

using state_now_result_t = std::tuple<sync_wait_valid_t, sync_wait_time_t, frame::state>;

class Frame;
using frame_t = std::shared_ptr<Frame>;
using frame_future = std::shared_future<frame_t>;
using frame_promise = std::promise<frame_t>;

class Frame : public std::enable_shared_from_this<Frame> {
private:
  Frame(uint8v &packet) noexcept;

public:
  static frame_t create(uint8v &packet) noexcept { return frame_t(new Frame(packet)); }

  // Digital Signal Analysis (hidden in .cpp)
  static void init();
  static void shutdown();

  // Public API

  state_now_result_t calc_sync_wait(AnchorLast &anchor);

  void find_peaks();

  bool parse();   // parse the deciphered frame into pcm data
  void process(); // async find peaks via dsp

  void mark_rendered() { state = frame::RENDERED; }

  bool silent() const noexcept {
    auto [left, right] = peaks;

    return Peaks::silence(left) && Peaks::silence(right);
  }

  //
  // state_now();
  //
  frame::state state_now(AnchorLast anchor, const Nanos &lead_time = InputInfo::lead_time());

  bool sync_wait_ok() const { return sync_wait > Nanos::min(); }

  // misc debug
  const string inspect(bool full = false);
  static const string inspect_safe(frame_t frame, bool full = false);
  void log_decipher() const;
  static constexpr csv moduleID() { return module_id; }

private:
  void set_sync_wait(const Nanos &diff) noexcept {
    if (diff != sync_wait) {
      sync_wait = diff;
    }

    // update sync_wait if changed
    // if (diff != sync_wait) {
    //   if (sync_wait != Nanos::min()) {
    //     __LOG0(LCOL01 " sync wait change, seq={} state={}/{} sync_wait={}/{}\n", module_id,
    //            "STATE_NOW", seq_num, prev_state(), state(), pet::humanize(sync_wait),
    //            pet::humanize(diff));
    //   }
    //   sync_wait = diff;
    // }

    //  __LOGX(LCOL01 " {}\n", module_id, csv("STATE_NOW"), inspect());
  }

public:
  // order dependent
  const Nanos created_at;
  Nanos sync_wait;
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
  ullong_t decipher_len{0};

  // decode frame
  int samples_per_channel = 0;
  int channels = 0;
  uint8v decoded;

  // populated by DSP
  std::tuple<peaks_t, peaks_t> peaks;

private:
  // order independent
  AnchorLast _anchor;

  // static string_view _render_mode;
  // static std::binary_semaphore _render;

public:
  static constexpr csv module_id{"FRAME"};
};

} // namespace pierre