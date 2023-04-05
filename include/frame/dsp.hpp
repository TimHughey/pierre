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

#include "base/uint8v.hpp"
#include "fft.hpp"
#include "frame.hpp"
#include "lcs/logger.hpp"

#include <array>
#include <boost/asio/error.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/system.hpp>
#include <memory>

namespace pierre {

namespace asio = boost::asio;
namespace sys = boost::system;
namespace errc = boost::system::errc;

using error_code = boost::system::error_code;
using work_guard_tp = asio::executor_work_guard<asio::thread_pool::executor_type>;

class Dsp {

public:
  Dsp() noexcept;
  ~Dsp() noexcept;

  void process(const frame_t frame, FFT &&left, FFT &&right) noexcept;

private:
  // order dependent
  const uint32_t concurrency_factor;
  const uint32_t thread_count;
  asio::thread_pool thread_pool;
  work_guard_tp guard;

private:
  void _process(const frame_t frame, FFT &&left, FFT &&right) noexcept;

public:
  static constexpr csv thread_prefix{"dsp"};
  static constexpr csv module_id{"frame.dsp"};
};

} // namespace pierre
