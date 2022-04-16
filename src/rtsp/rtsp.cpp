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

Rtsp::Rtsp(sHost _host) : host(_host), service_name(_host->serviceName()) {
  // maybe more later
}

Rtsp::~Rtsp() {
  // need cleanup code
}

void Rtsp::start() {
  fmt::print("\nPierre {:>25}\n\n", host->firmwareVerson());

  // create the Service for Rtsp
  service = Service::create(host);

  // create and start mDNS
  mdns = mDNS::create(service);
  mdns->start();

  // create and start Nptp
  nptp = Nptp::create(service);
  nptp->start();

  // create the AesCtx
  _aes_ctx = AesCtx::create(host->deviceID());

  // bind to AirPlay2 port and accept connectinon
  _thread = std::thread([this]() { runLoop(); });

  // save the native handle of the Rtsp thread
  _handle = _thread.native_handle();
}

void Rtsp::runLoop() {
  // NOTE: a IPv6 endpoint accepts both IPv6 and IPv4
  tcp::endpoint endpoint{tcp::v6(), service->basePort()};

  // setup the tcp_acceptor - NOTE: this becomes 'work' for the ioservice
  _acceptor = new tcp_acceptor{_ioservice, endpoint};

  // tell the acceptor to listen
  _acceptor->listen();

  // create a co-routine for ioservice
  // NOTE: ioservice will run this work until it returns
  spawn(_ioservice, [this](yield_context yield) { doAccept(yield); });

  // run the queued work (accepting connections)
  _ioservice.run();
}

void Rtsp::doAccept(yield_context yield) {
  do {
    _sockets.emplace_back(_ioservice);
    _acceptor->async_accept(_sockets.back(), yield);

    // add more work to the ioservice
    spawn(_ioservice, // lamba
          [this](yield_context yield) {
            auto &socket = _sockets.back();

            doRead(_sockets.back(), yield);
          });

  } while (true);
}

void Rtsp::doRead(tcp::socket &socket, yield_context yield) {
  auto request = rtsp::Request::create(_aes_ctx, service);

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