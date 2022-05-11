/*
    Pierre - Custom Light Show for Wiss Landing
    Copyright (C) 2022  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#pragma once

#include "reply/reply.hpp"

#include <array>
#include <cctype>
#include <memory>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace pierre {
namespace airplay {
namespace reply {

class FairPlay : public Reply {
public:
  FairPlay() = default;

  bool populate() override;

private:
  void payload1(uint8_t mode);
  void payload2();

private:
  // NOTE: these are all magic numbers; someday hunt down what they mean
  static const size_t vsn_idx = 4;
  static const size_t mode_idx = 14;
  static const size_t type_idx = 5;
  static const size_t seq_idx = 6;

  static const uint8_t setup_msg_type = 1;
  static const uint8_t setup1_msg_seq = 1;
  static const uint8_t setup2_msg_seq = 3;
  static const uint8_t setup2_suffix_len = 20;
};

} // namespace reply
} // namespace airplay
} // namespace pierre