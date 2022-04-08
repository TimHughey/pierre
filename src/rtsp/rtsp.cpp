/*
    Pierre - Custom Light Show for Wiss Landing
    Copyright (C) 2022  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#include <fmt/format.h>
#include <iostream>
#include <string_view>

#include "rtsp/reply.hpp"
#include "rtsp/request.hpp"
#include "rtsp/rtsp.hpp"

using namespace boost;
using namespace boost::asio;
using namespace boost::asio::ip;
using namespace boost::system;

namespace pierre {

using namespace rtsp;
using boost::system::error_code;

void Rtsp::run() {
  tcp::endpoint endpoint{tcp::v4(), _port};

  _acceptor = new tcp_acceptor{_ioservice, endpoint};

  _acceptor->listen();

  spawn(_ioservice, [this](yield_context yield) { doAccept(yield); });

  _ioservice.run();
}

void Rtsp::doAccept(yield_context yield) {

  for (;;) {

    _sockets.emplace_back(_ioservice);
    _acceptor->async_accept(_sockets.back(), yield);

    spawn(_ioservice, [this](yield_context yield) { doRead(_sockets.back(), yield); });
  }
}

void Rtsp::doRead(tcp::socket &socket, yield_context yield) {

  auto request = rtsp::Request::create();

  auto &dest = request->buffer();
  mutable_buffer buf(dest.data(), dest.size());

  socket.async_receive(buf, 0, // lamba
                       [this, request, &socket, yield](const error_code &ec, size_t bytesp) {
                         if (ec == system::errc::success) {
                           request->sessionStart(bytesp, ec.message());

                           session(socket, request, yield);

                           doRead(socket, yield);
                         } else {
                           fmt::print("socket shutdown: {}\n", ec.message());
                           socket.shutdown(tcp::socket::shutdown_both);
                         }
                       });
}

void Rtsp::session(tcp::socket &socket, auto request, yield_context yield) {
  using enum Request::DumpKind;

  request->parse();
  request->dump(HeadersOnly);

  // if content didn't arrive in the first packet and there's content to
  // load do so now
  if (request->shouldLoadContent()) {
    auto &dest = request->content();

    error_code ec;
    auto bytes = socket.receive(buffer(dest), 0, ec);

    if (ec != system::errc::success) {
      request->contentError(ec.message());
    }
  }

  auto reply = Reply::create(request);
  const auto &packet = reply->build();

  if (packet.size() > 0) {
    socket.send(buffer(packet.data(), packet.size()));
  }
}

void Rtsp::doWrite(tcp::socket &socket, yield_context yield) {
  std::time_t now = std::time(nullptr);
  std::string data = std::ctime(&now);

  async_write(socket, buffer(data), yield);
  socket.shutdown(tcp::socket::shutdown_both);
}
} // namespace pierre