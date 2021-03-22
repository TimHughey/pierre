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

#include <iostream>
#include <set>
#include <thread>
#include <vector>

#include <boost/asio.hpp>

#include "dmx/net.hpp"
#include "dmx/producer.hpp"
#include "local/types.hpp"

namespace pierre {
namespace dmx {

class Render {
public:
  struct Config {
    string_t host = string_t("test-with-devs.ruth.wisslanding.com");
    string_t port = string_t("48005");
  };

public:
  typedef std::set<std::shared_ptr<Producer>> producers;
  typedef std::error_code error_code;

public:
  Render(const Config &cfg);

  void addProducer(std::shared_ptr<Producer> producer) {
    _producers.insert(producer);
  }

  std::shared_ptr<std::thread> run();

private:
  void stream();

private:
  Config _cfg;

  io_context _io_ctx;
  Net _net;
  Frame _frame;

  producers _producers;
};
} // namespace dmx
} // namespace pierre

#endif
