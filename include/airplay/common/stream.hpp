
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

#pragma once

#include "core/typedefs.hpp"

namespace pierre {
namespace airplay {
namespace stream {

enum class cat : uint8_t { unspecified = 10, ptp_stream, ntp_stream, remote_control };
enum class type : uint8_t { none = 20, realtime, buffered };
enum class timing : uint8_t { none = 30, ntp, ptp };

constexpr uint64_t typeBuffered() { return 103; }
constexpr uint64_t typeRealTime() { return 96; }

} // namespace stream

class Stream {
public:
  Stream() = default;                     // allow default placeholder
  Stream(csv timing_protocol);            // create using timing_protocol
  Stream(const Stream &stream) = default; // copy construct (simple memeber data)
  Stream(Stream &&stream) = default;      // move construct

  Stream &operator=(const Stream &rhs) = default; // copy assignment
  Stream &operator=(Stream &&rhs) = default;      // move assignment

  Stream &buffered();
  Stream &realtime();

  auto category() const { return _cat; }

  bool isNtpStream() const { return _cat == stream::cat::ntp_stream; }
  bool isPtpStream() const { return _cat == stream::cat::ptp_stream; }
  bool isRemote() const { return _cat == stream::cat::remote_control; }

  uint64_t typeVal() const;

private:
  stream::cat _cat = stream::cat::unspecified;
  stream::type _type = stream::type::none;
  stream::timing _timing = stream::timing::none;
};

} // namespace airplay
} // namespace pierre