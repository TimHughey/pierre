/*
    Pierre - Custom Light Show for Wiss Landing
    Copyright (C) 2021  Tim Hughey

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

#ifndef _pierre_audio_net_hpp
#define _pierre_audio_net_hpp

#include <array>
#include <boost/asio.hpp>
#include <thread>

#include "audio/samples.hpp"

namespace pierre {
namespace audio {

constexpr size_t net_packet_size = 1024;

class RawOut : public Samples {
public:
  using string = std::string;
  using thread = std::thread;
  using spThread = std::shared_ptr<thread>;

public:
  typedef boost::asio::io_context io_context;
  typedef boost::asio::ip::udp::endpoint endpoint;
  typedef boost::asio::ip::udp::socket socket;
  typedef boost::system::error_code error_code;

  typedef std::array<uint8_t, net_packet_size> RawPacket;

  class Client {
  public:
    Client(io_context &io_ctx);
    ~Client() { _socket.close(); }

    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;

    void send(const RawPacket &data, endpoint &end_pt);

  private:
    socket _socket;
  };

public:
  RawOut(const string &dest, const string &port);

  RawOut(const RawOut &) = delete;
  RawOut &operator=(const RawOut &) = delete;

  spThread run();

private:
  void stream();

private:
private:
  error_code _ec;
  io_context _io_ctx;
  endpoint _dest_endpoint;

  bool _shutdown = false;

  Client _client = Client(_io_ctx);
  RawPacket _packet;
};

} // namespace audio

} // namespace pierre

#endif
