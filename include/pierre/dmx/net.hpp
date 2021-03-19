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

#include <iostream>

#include <boost/array.hpp>
#include <boost/asio.hpp>

#include "dmx/producer.hpp"
#include "dmx/types.hpp"
#include "local/types.hpp"

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
  Net(io_context &io_ctx, const string_t &host, const string_t &port);

  // bool connect();
  // bool connected() { return _socket.is_open(); }
  // bool read(rx_data &data);
  bool write(const UpdateInfo &info);

  void shutdown();

private:
  string_t _host;
  string_t _port;

  bool _connected = false;

  endpoints _endpoints;
  endpoint _dest;
  socket _socket;

  struct {
    struct {
      size_t bytes;
    } msgpack;
  } stats;

  size_t _msgpack_bytes;
};

} // namespace dmx
} // namespace pierre

#endif
