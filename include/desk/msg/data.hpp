// Pierre
// Copyright (C) 2022 Tim Hughey
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

#include "base/types.hpp"
#include "desk/msg/out.hpp"

#include <algorithm>
#include <vector>

namespace ranges = std::ranges;

namespace pierre {
namespace desk {

/// @brief Desk DataMsg encapsulation
class DataMsg : public MsgOut {
public:
  static constexpr uint32_t frame_len{12};

public:
  /// @brief Construct a new outbound Desk message
  /// @param seq_num Frame sequence number
  /// @param silence Is this message silence
  DataMsg(const seq_num_t seq_num, bool silence) noexcept
      : desk::MsgOut(desk::DATA),  //
        seq_num{seq_num},          // grab the frame seq_num
        silence{silence},          // grab if the frame is silent
        dmx_frame(frame_len, 0x00) // init the dmx frame
  {
    add_kv(desk::SEQ_NUM, seq_num);
    add_kv(desk::SILENCE, silence);
  }

  ~DataMsg() noexcept {} // prevent default copy/move

  DataMsg(DataMsg &&m) = default;           // allow move construct
  DataMsg &operator=(DataMsg &&) = default; // allow move assignment

public:
  /// @brief Raw pointer direct data access
  /// @return Raw uint8_t pointer
  uint8_t *frame() noexcept { return dmx_frame.data(); }

  void noop() noexcept {}

protected:
  /// @brief DataMsg specific serialization
  /// @param doc Reference to JsonDocument
  void serialize_hook(JsonDocument &doc) noexcept override {
    auto dframe = doc.createNestedArray(desk::FRAME);
    std::for_each(dmx_frame.begin(), dmx_frame.end(), [&](auto b) mutable { dframe.add(b); });
  }

private:
  const seq_num_t seq_num;
  const bool silence;
  std::vector<uint8_t> dmx_frame;

public:
  MOD_ID("desk.msg.data");
};

} // namespace desk
} // namespace pierre
