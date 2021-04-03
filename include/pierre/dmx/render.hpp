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

#ifndef _pierre_dmx_render_hpp
#define _pierre_dmx_render_hpp

#include <set>
#include <string>
#include <thread>

#include <boost/asio.hpp>

#include "dmx/net.hpp"
#include "dmx/producer.hpp"

namespace pierre {
namespace dmx {

class Render {
public:
  using string = std::string;
  using thread = std::thread;
  using spThread = std::shared_ptr<thread>;

  typedef std::set<std::shared_ptr<Producer>> Producers;
  typedef std::error_code error_code;

public:
  struct Config {
    string host = string("test-with-devs.ruth.wisslanding.com");
    string port = string("48005");
  };

public:
  Render(const Config &cfg);

  void addProducer(std::shared_ptr<Producer> producer) {
    _producers.insert(producer);
  }

  spThread run();

private:
  void stream();

private:
  Config _cfg;

  io_context _io_ctx;
  Net _net;
  Frame _frame;

  Producers _producers;
};
} // namespace dmx
} // namespace pierre

#endif
