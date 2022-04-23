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

#include <array>
#include <fmt/format.h>
#include <source_location>

#include "rtp/control.hpp"

using namespace boost;
using namespace boost::asio;
using namespace boost::asio::ip;
using namespace boost::system;

namespace pierre {
namespace rtp {

Control::Control() : _socket(_ioservice, udp_endpoint(udp::v6(), _port)) {
  // more later
}

Control::~Control() {
  // more later
}

uint16_t Control::localPort() {
  _port = _socket.local_endpoint().port();

  return _port;
}

void Control::recvPacket(yield_context yield) {
  std::array<uint8_t, 4096> packet{0};

  auto receive = [this, &packet, yield](const error_code &ec, size_t bytesp) {
    if (ec != system::errc::success) {
      const auto &msg = ec.message();
      fmt::print("control::Control::recvPacket(): socket shutdown: {}\n", msg);

      _socket.shutdown(udp::socket::shutdown_both);

      return;
    }

    const std::source_location loc = std::source_location::current();

    auto remote_ip = _remote_endpoint.address().to_string();

    fmt::print("{} remote_ip={} bytes={}", loc.function_name(), remote_ip, bytesp);
    recvPacket(yield);
  };

  mutable_buffer buf(packet.data(), packet.size());

  _socket.async_receive_from(buf, _remote_endpoint, receive);
}

void Control::runLoop() {
  // add some work to the ioservice
  spawn(_ioservice, [&](yield_context yield) {
    _port_promise.set_value(localPort());

    recvPacket(yield);
  });

  // run the queued work (accepting connections)
  _ioservice.run();
}

PortFuture Control::start() {
  _thread = std::thread([&]() { runLoop(); });
  _handle = _thread.native_handle();

  pthread_setname_np(_handle, "RTP Control");

  return _port_promise.get_future();
}

} // namespace rtp
} // namespace pierre
