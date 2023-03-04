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
#include "desk/msg/kv.hpp"
#include "frame/frame.hpp"

#include <variant>
#include <vector>

namespace pierre {
namespace desk {

class DataMsg {
private:
  // note: these are the only types that Units set
  using val_t = std::variant<uint32_t, float, bool>;

  struct key_val_entry {
    string key;
    val_t val;
  };

public:
  DataMsg(frame_t frame) noexcept
      : seq_num{frame->seq_num},  // grab the frame seq_num
        silence{frame->silent()}, // grab if the frame is silent
        dmx_frame(16, 0x00)       // init the dmx frame
  {}

  ~DataMsg() noexcept {} // prevent default copy/move

  DataMsg(DataMsg &&m) = default;           // allow move construct
  DataMsg &operator=(DataMsg &&) = default; // allow move assignment

public:
  void add(auto key, auto &&val) noexcept { key_vals.emplace_back(key_val_entry{key, val}); }

  uint8_t *dmxFrame() noexcept { return dmx_frame.data(); }

  void noop() noexcept {}

  void populate_doc(auto &doc) noexcept {
    doc[desk::SEQ_NUM] = seq_num;
    doc[desk::SILENCE] = silence;

    auto dframe = doc.createNestedArray("dframe");
    for (uint8_t byte : dmx_frame) {
      dframe.add(byte);
    }

    for (const auto &entry : key_vals) {
      // visit each entry value and add them to the document at the entry key
      std::visit([&](auto &&_val) { doc[entry.key] = _val; }, entry.val);
    }
  }

private:
  const seq_num_t seq_num;
  const bool silence;
  std::vector<uint8_t> dmx_frame;
  std::vector<key_val_entry> key_vals;

public:
  static constexpr csv module_id{"desk.msg.data"};
};

} // namespace desk
} // namespace pierre
