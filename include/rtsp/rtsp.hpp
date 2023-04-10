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

#include "desk/fdecls.hpp"
#include "frame/fdecls.hpp"
#include "rtsp/fdecls.hpp"
#include "rtsp/sessions.hpp"

#include <boost/asio/error.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/system.hpp>
#include <cstdint>
#include <memory>

namespace pierre {

namespace asio = boost::asio;
namespace sys = boost::system;
namespace errc = boost::system::errc;

using error_code = boost::system::error_code;
using ip_tcp = boost::asio::ip::tcp;
using tcp_acceptor = boost::asio::ip::tcp::acceptor;
using tcp_endpoint = boost::asio::ip::tcp::endpoint;
using tcp_socket = boost::asio::ip::tcp::socket;

namespace shared {
extern std::unique_ptr<Rtsp> rtsp;
}

class Rtsp {
public:
  Rtsp() noexcept;
  ~Rtsp() noexcept;

  /// @brief Accepts RTSP connections and starts a unique session for each
  void async_accept() noexcept;

private:
  // order dependent
  const int thread_count;
  asio::thread_pool thread_pool;
  tcp_acceptor acceptor;
  std::unique_ptr<rtsp::Sessions> sessions;
  std::unique_ptr<MasterClock> master_clock;
  std::unique_ptr<Desk> desk;

  // order independent
  static constexpr uint16_t LOCAL_PORT{7000};

public:
  static constexpr csv module_id{"rtsp"};
};

} // namespace pierre
