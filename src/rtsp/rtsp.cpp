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
#include <boost/system/error_code.hpp>
#include <fmt/format.h>
#include <iostream>
#include <pthread.h>

#include "core/service.hpp"
#include "rtp/rtp.hpp"
#include "rtsp/aes_ctx.hpp"
#include "rtsp/reply.hpp"
#include "rtsp/request.hpp"
#include "rtsp/rtsp.hpp"

namespace pierre {
using namespace boost;
using namespace boost::asio;
using namespace boost::asio::ip;
// using namespace boost::system;
using namespace rtsp;

Rtsp::Rtsp(sHost _host)
    : host(_host),                               // local copy of Host shared ptr
      service(Service::create(host)),            // create Service
      mdns(mDNS::create(service)),               // create mDNS
      nptp(Nptp::create(service)),               // create "Not Quite PTP"
      rtp(Rtp::create()),                        // create Real Time Protocol
      _aes_ctx(AesCtx::create(host->deviceID())) // create AES ctx
{
  // maybe more later
}

Rtsp::~Rtsp() {
  fmt::print("Rtsp destructor called\n");
  // need cleanup code
}

void Rtsp::start() {
  fmt::print("\nPierre {:>25}\n\n", host->firmwareVerson());

  // start network services
  mdns->start();
  nptp->start();
  rtp->start();

  // spin up thread to handle AirPlay2 comms
  _thread = std::thread([this]() { runLoop(); });

  // save the native handle of the Rtsp thread
  _handle = _thread.native_handle();
  pthread_setname_np(_handle, "RTSP");
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

            doRead(socket, yield);
          });

  } while (true);
}

void Rtsp::doRead(tcp::socket &socket, yield_context yield) {
  using error_code = system::error_code;

  auto request = rtsp::Request::create(_aes_ctx, service);

  std::array<mutable_buffer, 1> buffers = {buffer(request->packet())};
  // auto buffers = std::array{buffer(request->packet())};

  socket.async_receive(buffers, 0,
                       // lamba
                       [this, request, &socket, yield](const error_code &ec, size_t rbytes) {
                         if (ec == system::errc::success) {
                           auto more_bytes = socket.available();

                           if (more_bytes) {
                             fmt::print("Rtsp::async_receive() more bytes={}\n", more_bytes);
                           }

                           auto &packet = request->packet();
                           _aes_ctx->decrypt(packet, rbytes); // NOP until cipher is established

                           request->sessionStart(rbytes, ec.message());

                           session(socket, request);

                           doRead(socket, yield);
                         } else {
                           fmt::print("socket shutdown: {}\n", ec.message());
                           socket.shutdown(tcp::socket::shutdown_both);
                         }
                       });
}

void Rtsp::session(tcp::socket &socket, auto request) {
  using enum Request::DumpKind;

  request->parse();
  // request->dump(RawOnly);

  // if content didn't arrive in the first packet and there's content to
  // load do so now
  if (request->shouldLoadContent()) {
    auto &packet = request->content();
    mutable_buffer buf(packet.data(), packet.size());

    fmt::print("rtsp: loading {} bytes...\n", buf.size());

    system::error_code ec;
    auto bytes = socket.receive(buf, 0, ec);

    if (ec != system::errc::success) {
      request->contentError(ec.message());
    }

    fmt::print("rtsp: loaded {} bytes successfully\n", bytes);
  }

  // create the reply to the request
  auto req_final = request->final();
  try {
    auto reply = Reply::create({.method = request->method(),
                                .path = request->path(),
                                .content = request->content(),
                                .headers = request->headers(),
                                .host = host,
                                .service = service,
                                .aes_ctx = _aes_ctx,
                                .mdns = mdns,
                                .nptp = nptp,
                                .rtp = rtp});

    auto &packet = reply->build();

    _aes_ctx->encrypt(packet);

    if (packet.size() > 0) {
      socket.send(buffer(packet.data(), packet.size()));
    }
  } catch (const std::runtime_error &error) {
    auto eptr = std::current_exception();
    request->dump(Request::DumpKind::RawOnly);
    rethrow_exception(eptr);
  }
}

void Rtsp::doWrite(tcp::socket &socket, yield_context yield) {
  std::time_t now = std::time(nullptr);
  std::string data = std::ctime(&now);

  async_write(socket, buffer(data), yield);
  socket.shutdown(tcp::socket::shutdown_both);
}
} // namespace pierre