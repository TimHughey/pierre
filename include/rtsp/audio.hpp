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
#include "base/uint8v.hpp"
#include "rtsp/ctx.hpp"

#include <memory>
#include <optional>

namespace pierre {
namespace rtsp {

class Audio : public std::enable_shared_from_this<Audio> {

public:
  Port port() noexcept { return acceptor.local_endpoint().port(); }
  std::shared_ptr<Audio> ptr() noexcept { return shared_from_this(); }

  static auto start(io_context &io_ctx, std::shared_ptr<Ctx> rtsp_ctx) noexcept {
    auto self = std::shared_ptr<Audio>(new Audio(io_ctx, rtsp_ctx));

    self->async_accept();

    return self;
  }

  void teardown() noexcept {
    [[maybe_unused]] error_code ec;

    acceptor.close(ec);
    sock.close(ec);
  }

private:
  Audio(io_context &io_ctx, std::shared_ptr<Ctx> rtsp_ctx) noexcept
      : io_ctx(io_ctx),                                         //
        rtsp_ctx(rtsp_ctx),                                     //
        acceptor{io_ctx, tcp_endpoint{ip_tcp::v4(), ANY_PORT}}, //
        sock(io_ctx),                                           //
        local_strand(io_ctx)                                    //
  {}

  // asyncLoop is invoked to:
  //  1. schedule the initial async accept
  //  2. after accepting a connection to schedule the next async connect
  //
  // with this in mind we accept an error code that is checked before
  // starting the next async_accept.  when the error code is not specified
  // assume this is startup and all is well.
  void async_accept() noexcept;

  void async_read_packet() noexcept;

private:
  // order dependent
  io_context &io_ctx;
  std::shared_ptr<Ctx> rtsp_ctx;
  tcp_acceptor acceptor;
  tcp_socket sock;
  strand local_strand;

  // order independent
  tcp_endpoint endpoint;
  uint8v packet_len;
  uint8v packet;

  static constexpr csv module_id{"rtsp.audio"};
};

} // namespace rtsp
} // namespace pierre
