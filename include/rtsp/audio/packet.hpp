
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

#include "base/logger.hpp"
#include "base/types.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <boost/asio/read_until.hpp>
#include <cinttypes>
#include <cstdint>
#include <iterator>
#include <tuple>

namespace pierre {
namespace rtsp {
namespace audio {

/// @brief asio::read_until function object for audio packets
///        based on example from asio docs: https://tinyurl.com/4kvfdd8b
class packet {
public:
  static constexpr std::ptrdiff_t PREFIX{sizeof(uint16_t)};

public:
  explicit packet() noexcept {}

  template <typename Iterator>
  std::pair<Iterator, bool> operator()(Iterator begin, Iterator end) const {
    static constexpr csv fn_id{"operator()"};

    // we need at least the first two bytes
    if (auto n = std::distance(begin, end); n > PREFIX) {

      // grab the hdr bytes and convert to local endianness
      const auto msb = static_cast<uint8_t>(*begin) << 8;
      const auto lsb = static_cast<uint8_t>(*(begin + 1));
      std::ptrdiff_t data_len = msb + lsb;

      // INFO_AUTO("n={:5} data_len={:5}\n", n, data_len);

      if (n >= (data_len + PREFIX)) {
        // we have a complete audio packet (hdr and data)
        return std::make_pair(begin + data_len, true);
      }
    }

    // not enough bytes to determine if we have a complete audio packet
    return std::make_pair(begin, false);
  }

public:
  static constexpr csv module_id{"rtsp.audio"};
};
} // namespace audio
} // namespace rtsp
} // namespace pierre

namespace boost::asio {
template <> struct is_match_condition<pierre::rtsp::audio::packet> : public std::true_type {};
} // namespace boost::asio