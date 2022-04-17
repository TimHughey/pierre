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

#include <fmt/format.h>

#include "rtp/event/receiver.hpp"

using namespace boost;
using namespace boost::asio;
using namespace boost::asio::ip;
using namespace boost::system;

namespace pierre {
namespace rtp {
namespace event {

Receiver::Receiver() {
  // more later
}

Receiver::~Receiver() {
  // more later
}

void Receiver::doAccept(yield_context yield) {
  do {
    _sockets.emplace_back(_ioservice);
    _acceptor->async_accept(_sockets.back(), yield);

    // add more work to the ioservice
    spawn(_ioservice, // lamba
          [this](yield_context yield) {
            auto &socket = _sockets.back();

            recvEvent(socket, yield);
          });

  } while (true);
}

void Receiver::recvEvent(tcp_socket &socket, yield_context yield) {
  std::array<uint8_t, 4096> packet{0};

  mutable_buffer buf(packet.data(), packet.size());

  fmt::print("Receiver::recvEvent(): accepted conn socket={}\n", socket.native_handle());

  socket.async_receive(buf, 0,
                       // lamba
                       [this, &packet, &socket, yield](const error_code &ec, size_t bytesp) {
                         if (ec != system::errc::success) {
                           const auto &msg = ec.message();
                           fmt::print("Receiver::recvEvent(): socket shutdown: {}\n", msg);

                           socket.shutdown(tcp::socket::shutdown_both);

                           return;
                         }

                         // got a packet, what type is it?
                         constexpr auto time_announce = 0xd7;
                         const auto type = packet[1];

                         if (type == time_announce) {
                           fmt::print("Receiver::recvEvent() Time Announce RTP packet ");
                           fmt::print("type={:#02x} bytes={}\n", type, bytesp);
                         } else {
                           fmt::print("Receiver::recvEvent() Unknown RTP packet ");
                           fmt::print("type={:#02x} bytes={}\n", type, bytesp);
                         }

                         recvEvent(socket, yield);
                       });
}

void Receiver::runLoop() {
  // NOTE: a IPv6 endpoint accepts both IPv6 and IPv4
  tcp::endpoint endpoint{tcp::v6(), _port};

  // setup the tcp_acceptor - NOTE: this becomes 'work' for the ioservice
  _acceptor = new tcp_acceptor{_ioservice, endpoint};
  _port = _acceptor->local_endpoint().port();
  _port_promise.set_value(_port);

  fmt::print("Receiver::runLoop(): _port={}\n", _port);

  // tell the acceptor to listen
  _acceptor->listen();

  // create a co-routine for ioservice
  // NOTE: ioservice will run this work until it returns
  spawn(_ioservice, [this](yield_context yield) { doAccept(yield); });

  // run the queued work (accepting connections)
  _ioservice.run();
}

PortFuture Receiver::start() {
  _thread = std::thread([this]() { runLoop(); });
  _handle = _thread.native_handle();

  fmt::print("Receiver::start() _handle={}\n", _handle);

  return _port_promise.get_future();
}

} // namespace event
} // namespace rtp
} // namespace pierre