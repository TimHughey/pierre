
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
//
//  This work based on and inspired by
//  https://github.com/mikebrady/nqptp Copyright (c) 2021--2022 Mike Brady.

#include "conn_info/stream.hpp"
#include "reply/dict_keys.hpp"

#include <fmt/format.h>
#include <utility>

namespace pierre {
namespace airplay {

using namespace stream;

Stream &Stream::buffered() {
  _type = type::buffered;

  return *this;
}

Stream &Stream::realtime() {
  _type = type::realtime;
  return *this;
}

uint64_t Stream::typeVal() const {
  uint64_t type = 0;

  switch (_type) {
  case stream::type::none:
    type = 0;
    break;

  case stream::type::buffered:
    type = 103;
    break;

  case stream::type::realtime:
    type = 96;
    break;
  }

  return type;
}

Stream::Stream(csv timing_protocol) {

  if (timing_protocol == reply::dv::PTP) {
    _cat = cat::ptp_stream;
    _timing = timing::ptp;
    return;
  }

  if (timing_protocol == reply::dv::NTP) {
    _cat = cat::ptp_stream;
    _timing = timing::ptp;
    return;
  }

  if (timing_protocol == reply::dv::NONE) {
    _cat = cat::remote_control;
  }
}

} // namespace airplay
} // namespace pierre