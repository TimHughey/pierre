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

#include "common/typedefs.hpp"

#include <cstdint>
#include <fmt/format.h>
#include <source_location>
#include <string>
#include <string_view>

namespace pierre {
namespace airplay {

struct StreamData {
  std::string audio_mode;
  uint8_t ct = 0;
  uint64_t conn_id = 0;
  uint32_t spf = 0;
  std::string key;
  bool supports_dynamic_stream_id;
  uint32_t audio_format = 0;
  std::string client_id;
  uint8_t type = 0;
  std::string active_remote;
  std::string dacp_id;
};

class StreamInfo {
public:
  StreamInfo() = default;

  // copy/move constructors (from StreamData)
  StreamInfo(const StreamData &ad);
  StreamInfo(const StreamData &&ad);

  // copy/move constructors (from StreamInfo)
  StreamInfo(const StreamInfo &ai);
  StreamInfo(StreamInfo &&ai);

  // copy/move assignment (from StreamData)
  StreamInfo &operator=(const StreamData &ad);
  StreamInfo &operator=(StreamData &&ad);

  // copy/move assignment (from StreamInfo)
  StreamInfo &operator=(const StreamInfo &ai);
  StreamInfo &operator=(StreamInfo &&ai);

  // access the SessionData
  const std::string_view key() const { return std::string_view(data.key); }

  void teardown();

  // utility
  void dump(const std::source_location loc = std::source_location::current());

private:
  StreamData data;

private:
  void __init();
};

} // namespace airplay
} // namespace pierre
