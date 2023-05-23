
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

#include "base/types.hpp"

#include <algorithm>
#include <array>
#include <boost/asio/read_until.hpp>
#include <cstdint>
#include <iterator>
#include <tuple>

namespace pierre {
namespace desk {
namespace async {

//
// matcher uses the msgpack spec to find the beginning and end
// of a complete message by identifying the encoded representation
// of the 'mt' and 'ma' keys.
//
// https://github.com/msgpack/msgpack/blob/master/spec.md
//

/// @brief asio completion handler for Desk Msgs,
///        detects the begin and start of a MsgPack encoded message
class matcher {
public:
  /// @brief Construct object
  explicit matcher() noexcept {}

  /// @brief Functor to find a complete MsgPack encoded message from asio::async_read stream.
  ///        Searches stream for the encoded key/val of { "ma" = 828 }.
  ///        The magic number of 828 represents the C64 cassette buffer starting memory address.
  /// @tparam Iterator
  /// @param begin Start of the range of bytes to examine
  /// @param end End of the range of bytes to examine
  /// @return boolean, true when MsgPack encoded message is found, false otherwise
  template <typename Iterator>
  std::pair<Iterator, bool> operator()(Iterator begin, Iterator end) const {

    const auto n = std::distance(begin, end);

    // first, do we have enough bytes to detect EOM?
    if (n < std::ssize(suffix)) return std::make_pair(begin, false);

    auto found = std::search(begin, end,                   //
                             suffix.begin(), suffix.end(), //
                             [](auto &in, auto &s) {       //
                               return static_cast<uint8_t>(in) == s;
                             });

    if (found == end) return std::make_pair(begin, false);

    return std::make_pair(found + std::ssize(suffix), true);
  }

private:
  // msgpack encoding of { "ma" = 828 }
  static constexpr std::array<uint8_t, 5> suffix{0x6d, 0x61, 0xcd, 0x03, 0x3c};
};
} // namespace async
} // namespace desk
} // namespace pierre

namespace boost::asio {
template <> struct is_match_condition<pierre::desk::async::matcher> : public std::true_type {};
} // namespace boost::asio