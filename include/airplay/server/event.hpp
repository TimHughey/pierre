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
#include "server/base.hpp"

#include <memory>
#include <optional>

namespace pierre {
namespace airplay {
namespace server {

class Event : public Base {
public:
  Event(io_context &io_ctx)
      : Base(SERVER_ID),                                       //
        io_ctx(io_ctx),                                        //
        acceptor{io_ctx, tcp_endpoint(ip_tcp::v4(), ANY_PORT)} //
  {}

  // asyncLoop is invoked to:
  //  1. schedule the initial async accept
  //  2. after accepting a connection to schedule the next async connect
  //
  // with this in mind we accept an error code that is checked before
  // starting the next async_accept.  when the error code is not specified
  // assume this is startup and all is well.
  void asyncLoop(const error_code ec_last = error_code()) override;

  Port localPort() override { return acceptor.local_endpoint().port(); }
  void teardown() override; // issue cancel to acceptor

private:
  // order dependent
  io_context &io_ctx;
  tcp_acceptor acceptor;

  // temporary holder of socket (io_ctx) while waiting for a connection
  std::optional<tcp_socket> socket;

  static constexpr auto SERVER_ID = "EVENT SERVER";
};

} // namespace server
} // namespace airplay
} // namespace pierre
