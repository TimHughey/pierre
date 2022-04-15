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

#include <array>
#include <fmt/format.h>
#include <iostream>
#include <pthread.h>
#include <string_view>

#include "core/service.hpp"
#include "rtsp/aes_ctx.hpp"
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

Rtsp::Rtsp(sService service, uint16_t port)
    : _thread{}, _port(port), _service(service) {
  _aes_ctx = AesCtx::create(_service->fetchVal(apDeviceID));
}

void Rtsp::start() {
  _thread = std::thread([this]() { runLoop(); });

  _handle = _thread.native_handle();
}

void Rtsp::runLoop() {
  // optional<boost::asio::io_service::work> work =
  // in_place(boost::ref(io_service));

  tcp::endpoint endpoint_v4{tcp::v4(), _port};
  _accept_v4 = new tcp_acceptor{_ioservice, endpoint_v4};

  tcp::endpoint endpoint_v6(tcp::v6(), _port);
  _accept_v6 = new tcp_acceptor{_ioservice, endpoint_v6};

  _accept_v4->listen();

  spawn(_ioservice, [this](yield_context yield) { doAccept(yield); });

  _ioservice.run();
}

void Rtsp::doAccept(yield_context yield) {
  do {
    _sockets.emplace_back(_ioservice);
    _accept_v4->async_accept(_sockets.back(), yield);

    // fmt::print("Rtsp::doAccept() spawning...\n");
    spawn(_ioservice, // lamba
          [this](yield_context yield) {
            auto &socket = _sockets.back();

            doRead(_sockets.back(), yield);
          });

  } while (true);
}

void Rtsp::doRead(tcp::socket &socket, yield_context yield) {
  auto request = rtsp::Request::create(_aes_ctx, _service);

  auto &packet = request->packet();
  packet.clear();

  mutable_buffer buf(packet.data(), packet.size());

  socket.async_receive(
      buf, 0, // lamba
      [this, request, &socket, yield](const error_code &ec, size_t bytesp) {
        if (ec == system::errc::success) {
          // NOTE this is a NOP until cipher is established
          auto &packet = request->packet();
          _aes_ctx->decrypt(packet, bytesp);

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
  // request->dump(RawOnly);

  // if content didn't arrive in the first packet and there's content to
  // load do so now
  if (request->shouldLoadContent()) {
    auto &packet = request->content();
    mutable_buffer buf(packet.data(), packet.size());

    fmt::print("rtsp: loading {} bytes...\n", buf.size());

    error_code ec;
    auto bytes = socket.receive(buf, 0, ec);

    if (ec != system::errc::success) {
      request->contentError(ec.message());
    }

    fmt::print("rtsp: loaded {} bytes successfully\n", bytes);
  }

  auto reply = Reply::create(request);
  auto &packet = reply->build();

  _aes_ctx->encrypt(packet);

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