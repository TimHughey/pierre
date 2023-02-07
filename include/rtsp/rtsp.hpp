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

#include "io/io.hpp"
#include "rtsp/sessions.hpp"

#include <any>
#include <cstdint>
#include <latch>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <vector>

namespace pierre {

class Desk;
class MasterClock;

namespace rtsp {
class Ctx;
} // namespace rtsp

class Rtsp {
public:
  Rtsp() noexcept;
  ~Rtsp() noexcept;

  /// @brief Accepts RTSP connections and starts a unique session for each
  void async_accept() noexcept;

private:
  // order dependent
  io_context io_ctx;
  tcp_acceptor acceptor;
  std::unique_ptr<rtsp::Sessions> sessions;
  std::unique_ptr<MasterClock> master_clock;
  std::unique_ptr<Desk> desk;
  const int thread_count;
  std::shared_ptr<std::latch> shutdown_latch;

  static constexpr uint16_t LOCAL_PORT{7000};

public:
  static constexpr csv module_id{"rtsp"};
};

} // namespace pierre
