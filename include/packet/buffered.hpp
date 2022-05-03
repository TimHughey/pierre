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

#include <array>
#include <boost/asio/buffer.hpp>
#include <cstdint>
#include <source_location>
#include <vector>

namespace pierre {
namespace packet {

class BufferedTCP {
public:
  enum BufferType : uint8_t { All = 0, Static, Dynamic };

public:
  BufferedTCP() {
    _buffer_.fill(0x00);
    _toq_ = _buffer_.data();
    _eoq_ = _buffer_.data();
  }

  auto dynamicBuffer() { return boost::asio::dynamic_buffer(_dyn_buffer_;) }
  auto staticBuffer() { return boost::asio::buffer(_buffer_, MAX_SIZE); }

  size_t maxSize() { return MAX_SIZE; }
  size_t occupancy() { return MAX_SIZE - _buffer_.size(); } // buffer bytes used

  uint8_t *toq() { return _toq_; } // top-of-queue ptr
  uint8_t *eoq() { return _eoq_; } // end-of-queue ptr

public:
  static constexpr size_t MAX_SIZE = 0x800000; // ap2 buffer max size

private:
  std::array<uint8_t, MAX_SIZE> _buffer_;
  std::vector<uint8_t> _dyn_buffer_;
  uint8_t *_toq_ = nullptr;
  uint8_t *_eoq_ = nullptr;
};

} // namespace packet
} // namespace pierre