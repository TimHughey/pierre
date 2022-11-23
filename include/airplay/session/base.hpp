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
#include "base/types.hpp"
#include "common/ss_inject.hpp"

#include <memory>

namespace pierre {
namespace airplay {
namespace session {

class Base {

public:
  Base(const Inject &di, csv module_id) noexcept
      : socket(std::move(di.socket)), //
        local_strand(di.io_ctx),      //
        module_id(module_id)          //
  {}

  virtual ~Base() = default;

  virtual void asyncLoop() = 0;

  bool isReady() const noexcept { return socket.is_open(); };

  virtual bool isReady(const error_code &ec) noexcept {
    auto rc = isReady();

    if (rc) {
      switch (ec.value()) {
      case errc::success:
        break;

      case errc::operation_canceled:
      case errc::resource_unavailable_try_again:
      case errc::no_such_file_or_directory:
        rc = false;
        break;

      default:
        INFO(module_id, "NOT READY", "socket={} {}\n", socket.native_handle(), ec.message());

        socket.shutdown(tcp_socket::shutdown_both);
        socket.close();
        rc = false;
      }
    }

    return rc;
  }

  virtual void shutdown() noexcept { teardown(); }
  virtual void teardown() noexcept {
    [[maybe_unused]] error_code ec;

    socket.cancel(ec);
  }

protected:
  // order dependent - initialized by constructor
  tcp_socket socket;
  strand local_strand;

public:
  string module_id; // used for logging
};

} // namespace session
} // namespace airplay
} // namespace pierre
