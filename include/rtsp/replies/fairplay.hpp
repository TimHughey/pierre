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

#include "rtsp/reply.hpp"

#include <array>
#include <memory>

namespace pierre {
namespace rtsp {

class FairPlay {
public:
  FairPlay(Request &request, Reply &reply) noexcept;

private:
  // NOTE: these are all magic numbers; someday hunt down what they mean
  static constexpr size_t vsn_idx{4};
  static constexpr size_t mode_idx{14};
  static constexpr size_t type_idx{5};
  static constexpr size_t seq_idx{6};

  static constexpr uint8_t setup_msg_type{1};
  static constexpr uint8_t setup1_msg_seq{1};
  static constexpr uint8_t setup2_msg_seq{3};
  static constexpr ptrdiff_t setup2_suffix_len{20};

public:
  static constexpr csv module_id{"FAIRPLAY"};
};

} // namespace rtsp
} // namespace pierre