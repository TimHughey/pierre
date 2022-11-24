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

#include <cstdint>
#include <memory>
#include <optional>

namespace pierre {

class Rtsp : public std::enable_shared_from_this<Rtsp> {
private:
  Rtsp(io_context &io_ctx)
      : io_ctx(io_ctx), acceptor{io_ctx, tcp_endpoint(ip_tcp::v4(), LOCAL_PORT)} //
  {}

public:
  static std::shared_ptr<Rtsp> init(io_context &io_ctx) noexcept {
    auto self = std::shared_ptr<Rtsp>(new Rtsp(io_ctx));

    self->async_loop();

    return self;
  }

  auto ptr() noexcept { return shared_from_this(); }

  // asyncLoop is invoked to:
  //  1. schedule the initial async accept
  //  2. after accepting a connection to schedule the next async connect
  //
  // with this in mind we accept an error code that is checked before
  // starting the next async_accept.  when the error code is not specified
  // assume this is startup and all is well.
  void async_loop(const error_code ec_last = io::make_error(errc::success)) noexcept;

  void shutdown() noexcept { teardown(); }

  void teardown() noexcept {
    // here we only issue the cancel to the acceptor.
    // the closing of the acceptor will be handled when
    // the error is caught by asyncLoop

    try {
      acceptor.cancel();
      acceptor.close();
    } catch (...) {
    }
  }

private:
  io_context &io_ctx;
  tcp_acceptor acceptor;

  std::optional<tcp_socket> sock_accept;

  static constexpr uint16_t LOCAL_PORT{7000};

public:
  static constexpr csv module_id{"RTSP"};
};

} // namespace pierre
