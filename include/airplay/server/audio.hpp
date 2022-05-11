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

#include "common/ss_inject.hpp"
#include "server/base.hpp"

#include <fmt/format.h>
#include <optional>

namespace pierre {
namespace airplay {
namespace server {

class Audio : public Base {
public:
  Audio(const Inject &inject)
      : di(inject),                                            // safe to save injected deps here
        io_ctx(inject.io_ctx),                                 // local listen port
        acceptor{io_ctx, tcp_endpoint(ip_tcp::v6(), ANY_PORT)} // create acceptor

  {}

  void asyncLoop() override;
  void asyncLoop(const error_code &ec) override;

  // ensure server is started and return the local endpoint port
  Port localPort() override { return acceptor.local_endpoint().port(); }

  void teardown() override {
    [[maybe_unused]] error_code ec;
    acceptor.cancel();
    acceptor.close(ec);
  }

private:
  void announceAccept(const auto handle) {
    if (false) { // log accepted connection
      auto fmt_str = FMT_STRING("{} accepted connection, handle={}\n");
      fmt::print(fmt_str, fnName(), handle);
    }
  }

private:
  // order dependent
  const Inject di; // make a copy
  io_context &io_ctx;
  tcp_acceptor acceptor;

  uint16_t port = 0;
  bool live = false;

  // temporary holder of socket (io_ctx) while waiting for
  // a connection
  std::optional<tcp_socket> socket;

  static constexpr uint16_t ANY_PORT = 0;
};

} // namespace server
} // namespace airplay
} // namespace pierre
