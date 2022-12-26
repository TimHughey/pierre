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

#include "base/io.hpp"
#include "base/logger.hpp"
#include "base/threads.hpp"

#include <cstdint>
#include <memory>
#include <optional>

namespace pierre {

class Rtsp : public std::enable_shared_from_this<Rtsp> {
private:
  Rtsp()
      : acceptor{io_ctx, tcp_endpoint(ip_tcp::v4(), LOCAL_PORT)},
        guard(asio::make_work_guard(io_ctx)) {}

  /// @brief Accepts RTSP connections and starts a unique session for each
  void async_accept() noexcept;

  auto ptr() noexcept { return shared_from_this(); }

public:
  /// @brief Creates and starts the RTSP connection listener and worker threads
  /// as specified in the external config
  static void init() noexcept;

  /// @brief Shuts down the RTSP listener and releases shared_ptr to self
  static void shutdown() noexcept {
    if (self.use_count() > 1) {
      auto s = self->ptr();     // grab a fresh shared_ptr (will reset self below)
      auto &io_ctx = s->io_ctx; // grab a ref to the io_ctx (will move s into lambda)

      asio::post(io_ctx, [s = std::move(s)]() {
        [[maybe_unused]] error_code ec;

        s->guard.reset(); // allow the io_ctx to complete when other work is finished
        s->acceptor.close(ec);
      });

      // reset the shared_ptr to ourself.  provided no other shared_ptrs exist
      // when the above post completes we will be deallocated
      self.reset();
    }
  }

private:
  // order dependent
  io_context io_ctx;
  tcp_acceptor acceptor;
  work_guard guard;

  // order independent
  static std::shared_ptr<Rtsp> self;
  std::optional<tcp_socket> sock_accept;
  Threads threads;

  static constexpr uint16_t LOCAL_PORT{7000};

public:
  static constexpr csv module_id{"RTSP"};
};

} // namespace pierre
