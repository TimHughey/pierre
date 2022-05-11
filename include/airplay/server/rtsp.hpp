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
#include "session/rtsp.hpp"

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <cstdint>
#include <memory>
#include <optional>

namespace pierre {
namespace airplay {
namespace server {

class Rtsp : public Base {
public:
  Rtsp(const Inject &di)
      : io_ctx(di.io_ctx), acceptor{io_ctx, tcp_endpoint(ip_tcp::v6(), _port)}, di(di) {
    // store a reference to the io_ctx and create the acceptor
    // see start()
  }

  void asyncLoop() override;
  void asyncLoop(const error_code &ec) override;

  Port localPort() override { return acceptor.local_endpoint().port(); }

  void teardown() override {
    acceptor.cancel();
    acceptor.close();
  }

private:
  io_context &io_ctx;
  tcp_acceptor acceptor;
  const Inject &di;

  std::optional<tcp_socket> socket;

  static constexpr uint16_t _port = 7000;
  static constexpr auto SERVER_ID = "RTSP";
};

} // namespace server
} // namespace airplay
} // namespace pierre