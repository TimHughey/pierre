
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

class matcher {
public:
  explicit matcher() noexcept {}

  template <typename Iterator>
  std::pair<Iterator, bool> operator()(Iterator begin, Iterator end) const {

    const auto n = std::distance(begin, end);

    // first, do we have enough bytes to detect EOM?
    if (n < std::ssize(suffix)) return std::make_pair(begin, false);

    auto found = std::search(begin, end,                         //
                             suffix.begin(), suffix.end(),       //
                             [](const auto &in, const auto &s) { //
                               return static_cast<uint8_t>(in) == s;
                             });

    if (found == end) return std::make_pair(begin, false);

    // ESP_LOGI("desk.async.matcher", "n=%d", n);

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