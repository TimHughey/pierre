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

#include "base/types.hpp"
#include "desk/fdecls.hpp"
#include "frame/fdecls.hpp"
#include "rtsp/fdecls.hpp"

#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system.hpp>
#include <cstdint>
#include <memory>
#include <vector>

namespace pierre {

namespace asio = boost::asio;
namespace sys = boost::system;
namespace errc = boost::system::errc;

using error_code = boost::system::error_code;
using ip_tcp = boost::asio::ip::tcp;
using strand_ioc = boost::asio::strand<boost::asio::io_context::executor_type>;
using tcp_acceptor = boost::asio::ip::tcp::acceptor;
using tcp_endpoint = boost::asio::ip::tcp::endpoint;
using tcp_socket = boost::asio::ip::tcp::socket;

using rtsp_ctx_ptr = std::unique_ptr<rtsp::Ctx>;

class Rtsp {
  static constexpr uint16_t LOCAL_PORT{7000};

public:
  Rtsp(asio::io_context &app_io_ctx, Desk *desk);
  ~Rtsp() noexcept; // must be in .cpp due to incomplete types

  /// @brief Accepts RTSP connections and starts a unique session for each
  void async_accept() noexcept;

  void close_all() noexcept;

  void set_live(rtsp::Ctx *ctx) noexcept;

private:
  // order dependent
  asio::io_context &io_ctx;
  Desk *desk{nullptr};
  tcp_acceptor acceptor;

  // order independent
  std::vector<rtsp_ctx_ptr> sessions;

public:
  static constexpr auto module_id{"rtsp"sv};
};

} // namespace pierre
