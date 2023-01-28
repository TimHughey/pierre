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

#include "base/threads.hpp"
#include "io/io.hpp"

#include <any>
#include <cstdint>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <vector>

namespace pierre {

namespace rtsp {
class Ctx;
} // namespace rtsp

class Rtsp : public std::enable_shared_from_this<Rtsp> {
private:
  Rtsp()
      : acceptor{io_ctx, tcp_endpoint(ip_tcp::v4(), LOCAL_PORT)},
        guard(asio::make_work_guard(io_ctx)) //
  {}

  /// @brief Accepts RTSP connections and starts a unique session for each
  void async_accept() noexcept;

  auto ptr() noexcept { return shared_from_this(); }

public:
  /// @brief Creates and starts the RTSP connection listener and worker threads
  /// as specified in the external config
  static void init() noexcept;

  /// @brief Shuts down the RTSP listener and releases shared_ptr to self
  static void shutdown() noexcept;

  static void live_session(const string &dacp_id, int64_t active_remote) noexcept;

private:
  // order dependent
  io_context io_ctx;
  tcp_acceptor acceptor;
  work_guard guard;

  // order independent
  static std::shared_ptr<Rtsp> self;
  Threads threads;
  std::vector<std::shared_ptr<rtsp::Ctx>> sessions;
  std::shared_mutex sessions_mtx;

  static constexpr uint16_t LOCAL_PORT{7000};

public:
  static constexpr csv module_id{"rtsp"};
};

} // namespace pierre
