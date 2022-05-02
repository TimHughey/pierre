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

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <source_location>
#include <string.h>
#include <string_view>
#include <vector>

#include "core/input_info.hpp"
#include "packet/queued.hpp"

namespace pierre {
namespace pcm {

class Buffer : public std::vector<uint8_t> {
public:
  using src_loc = std::source_location;
  typedef const std::string_view csv;
  typedef const char *ccs;

public:
  Buffer();

  // auto buffer() { return boost::asio::dynamic_buffer(_packet); }
  // bool loaded(size_t rx_bytes);

  // ccs raw() const { return (ccs)_packet.data(); }
  // void reset();
  // size_t size() const { return _packet.size(); }

  // SeqNum sequenceNum() const { return _seq_num; }
  // uint32_t timestamp() const { return _timestamp; }
  // Type type() const { return _type; }
  // bool valid() const { return _valid; }
  // const csv view() const { return csv(raw(), size()); }

  // misc helpers, debug, etc.
  ccs fnName(src_loc loc = src_loc::current()) const { return loc.function_name(); }

private:
  static constexpr size_t STD_BUFFER_SIZE = 8 * 1024 * 1024;
};

} // namespace pcm
} // namespace pierre
