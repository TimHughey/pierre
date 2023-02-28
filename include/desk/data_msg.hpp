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

#include "base/elapsed.hpp"
#include "base/pet_types.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "desk/msg.hpp"
#include "frame/frame.hpp"

#include <iterator>

namespace pierre {
namespace desk {

class DataMsg : public Msg {

public:
  DataMsg(frame_t frame) noexcept;
  ~DataMsg() noexcept {} // prevent default copy/move

  DataMsg(DataMsg &&m) = default;           // allow move construct
  DataMsg &operator=(DataMsg &&) = default; // allow move assignment

public:
  uint8_t *dmxFrame() { return dmx_frame.data(); }

  void finalize() override {
    auto dframe = doc.createNestedArray("dframe");
    for (uint8_t byte : dmx_frame) {
      dframe.add(byte);
    }
  }

  void noop() noexcept {}

  // misc debug
  string inspect() const noexcept override {
    string msg;

    fmt::format_to(std::back_inserter(msg), "silence={} packed_len={} dmx_len={}\n", //
                   silence, measureMsgPack(doc), dmx_frame.size());

    serializeJsonPretty(doc, msg);

    return msg;
  }

private:
  // order dependent
  static constexpr csv TYPE{"data"};
  uint8v dmx_frame;
  bool silence{false};

public:
  static constexpr csv module_id{"desk.dmx_data_msg"};
};

} // namespace desk
} // namespace pierre
