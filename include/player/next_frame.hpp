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

#include "core/typedefs.hpp"
#include "packet/basic.hpp"
#include "player/flush_request.hpp"
#include "player/peaks.hpp"
#include "player/rtp.hpp"
#include "rtp_time/rtp_time.hpp"

#include <array>
#include <memory>
#include <utility>

namespace pierre {

struct NextFrame {
  player::shRTP frame;
  bool spool_end = false;

  const string toString() const {
    string msg;
    auto m = std::back_inserter(msg);

    fmt::format_to(m, "{} ", moduleId);
    fmt::format_to(m, "{:^12} diff={:0.1} ", (frame.use_count() > 0) ? csv("GOOD") : csv("BAD"), //
                   std::chrono::duration_cast<MillisFP>(diff_ns));

    fmt::format_to(m, "played={:<5} skipped={:<5} total={:<5} ", played, skipped, total);

    if (silence) {
      fmt::format_to(m, "SILENCE ");
    }

    if (spool_end) {
      fmt::format_to(m, "SPOOL END");
    }

    return msg;
  }

  static constexpr auto moduleId = csv("NEXT FRAME");
};

} // namespace pierre