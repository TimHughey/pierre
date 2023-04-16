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

#include "base/asio.hpp"
#include "base/logger.hpp"
#include "base/uint8v.hpp"
#include "fft.hpp"
#include "frame.hpp"

#include <boost/asio/error.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/system.hpp>
#include <memory>

namespace pierre {

using work_guard_ioc = asio::executor_work_guard<asio::io_context::executor_type>;

class Dsp {

public:
  Dsp() noexcept;
  ~Dsp() noexcept { io_ctx.stop(); }

  // we pass frame as const since we are not taking ownership
  void process(const frame_t frame, FFT &&left, FFT &&right) noexcept;

private:
  // order dependent
  asio::io_context io_ctx;
  work_guard_ioc work_guard; // Dsp processes frames as received, needs work guard

private:
  void _process(const frame_t frame, FFT &&left, FFT &&right) noexcept;

public:
  static constexpr csv module_id{"frame.dsp"};
};

} // namespace pierre
