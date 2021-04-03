/*
    Pierre - Custom Light Show via DMX for Wiss Landing
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

#ifndef _pierre_dmx_net_hpp
#define _pierre_dmx_net_hpp

#include <string>

#include <boost/array.hpp>
#include <boost/asio.hpp>

#include "dmx/packet.hpp"
#include "dmx/producer.hpp"

namespace pierre {
namespace dmx {
typedef std::vector<uint8_t> tx_data;
typedef std::vector<uint8_t> rx_data;
typedef boost::asio::io_context io_context;
typedef boost::asio::ip::udp::socket socket;
typedef boost::asio::ip::udp::resolver resolver;
typedef boost::asio::ip::udp::endpoint endpoint;
typedef boost::system::error_code error_code;
typedef boost::asio::ip::udp::resolver::results_type endpoints;

class Net {
public:
  using string = std::string;

public:
  Net(io_context &io_ctx, const string &host, const string &port);

  bool write(Packet &packet);

  void shutdown();

private:
  string _host;
  string _port;

  bool _connected = false;

  endpoints _endpoints;
  endpoint _dest;
  socket _socket;

  struct {
    struct {
      size_t bytes;
    } msgpack;
  } stats;
};

} // namespace dmx
} // namespace pierre

#endif
