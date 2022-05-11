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

#include "server/base.hpp"

#include <memory>
#include <optional>

namespace pierre {
namespace airplay {
namespace server {

class Event : public Base {
public:
  Event(const Inject &di)
      : io_ctx(di.io_ctx),                                     // the io_context
        di(di),                                                // connection information
        acceptor(io_ctx, tcp_endpoint(ip_tcp::v6(), ANY_PORT)) // create acceptor
  {}

  // start accepting connections
  void asyncLoop() override;

  // check error code and either add more work or gracefully exit
  void asyncLoop(const error_code &ec) override;

  // ensure server is started and return the local endpoint port
  uint16_t localPort() override { return acceptor.local_endpoint().port(); }

  void teardown() override {
    [[maybe_unused]] error_code ec;
    acceptor.close(ec);
  }

private:
  // order dependent
  io_context &io_ctx;
  const Inject &di;
  tcp_acceptor acceptor;

  bool live = false;

  // temporary holder of socket (io_ctx) while waiting for a connection
  std::optional<tcp_socket> socket;

  static constexpr uint16_t ANY_PORT = 0;
};

} // namespace server
} // namespace airplay
} // namespace pierre
